#!/usr/bin/env python3
"""Owned actual service: exclusion transactions under kernel lookup uncertainty."""
import argparse,hashlib,json,os,resource,socket,sqlite3,subprocess,time
from pathlib import Path
A=argparse.ArgumentParser();A.add_argument('--service',required=True);A.add_argument('--output',required=True);args=A.parse_args();P=Path.cwd();O=Path(args.output).resolve();O.mkdir()
B=Path(args.service).resolve()
probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe'
raw=[];cases=[];begin=time.monotonic_ns()
def run(kind):
 out=O/kind;out.mkdir();parent=out/'parent';parent.mkdir();root=parent/'corpus';root.mkdir()
 a=root/'source.txt';b=root/'excluded.txt'
 abody='F1-KEEP-78164 genuine serialized sibling';bbody='F1-REMOVE-95237 genuine requested exclusion'
 a.write_text(abody);b.write_text(bbody);data=out/'data';sock=O/(kind+'.sock')
 log=(out/'service.log').open('wb');p=subprocess.Popen([str(B),'--data-dir',str(data),'--socket',str(sock),'--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','2'],stdout=log,stderr=log)
 c=None;buf=b'';rid=0;workers=set();oldlimit=None;result={};events=[]
 def req(op,**args):
  nonlocal rid,buf
  rid+=1;c.sendall(json.dumps(dict(id=str(rid),op=op,**args)).encode()+b'\n')
  while b'\n' not in buf:
   chunk=c.recv(65536)
   if not chunk:raise RuntimeError('service closed')
   buf+=chunk
  line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==str(rid)
  events.append(dict(clock_ns=time.monotonic_ns(),op=op,args=args,reply=r));return r
 def wait(pred):
  for _ in range(1500):
   s=req('status')['status']
   for f in Path(f'/proc/{p.pid}/task').glob('*/children'):workers.update(map(int,f.read_text().split()))
   if pred(s):return s
   time.sleep(.02)
  raise RuntimeError('status timeout')
 def serialized():
  with sqlite3.connect(data/'index.sqlite') as db:
   return dict(pages=db.execute('select path,text from pages order by path').fetchall(),revision=db.execute("select value from meta where key='index_revision'").fetchone()[0],exclusions=[(x[0].decode() if isinstance(x[0],bytes) else x[0],) for x in db.execute('select exclusions from roots')])
 def watches():
  result={}
  for f in Path(f'/proc/{p.pid}/fdinfo').iterdir():
   try:v=f.read_text()
   except FileNotFoundError:continue
   if 'inotify wd:' in v:result[f.name]=v
  return result
 def query():
  return req('search',text='F1-KEEP-78164')
 try:
  for _ in range(500):
   try:c=socket.socket(socket.AF_UNIX);c.settimeout(90);c.connect(str(sock));break
   except OSError:c.close();c=None;time.sleep(.02)
  assert c;wait(lambda s:s['ready']);req('add_root',path=str(root));wait(lambda s:not s['busy'] and s['indexed']==2)
  baseline=serialized();positive=query();req('pause');wait(lambda s:s['paused']);time.sleep(.7);baseline_status=req('status')['status'];before_watches=watches()
  if kind=='EACCES':
   parent.chmod(0);witness=dict(parent_mode=oct(parent.stat().st_mode&0o777))
   try:a.open().close();witness['reader_errno']=None
   except OSError as e:witness['reader_errno']=e.errno;witness['reader_error']=str(e)
  else:
   oldlimit=resource.prlimit(p.pid,resource.RLIMIT_NOFILE)
   fdmap={int(f.name):os.readlink(f) for f in Path(f'/proc/{p.pid}/fd').iterdir()}
   firstfree=next(i for i in range(oldlimit[0]) if i not in fdmap)
   resource.prlimit(p.pid,resource.RLIMIT_NOFILE,(firstfree,oldlimit[1]))
   witness=dict(pid=p.pid,original_limit=oldlimit,applied_limit=resource.prlimit(p.pid,resource.RLIMIT_NOFILE),fdmap=fdmap,all_slots_below_limit_occupied=all(i in fdmap for i in range(firstfree)))
  scan=req('scan');after_scan=serialized();scan_status=req('status')['status'];uncertain_query=query() if kind=='EACCES' else None
  unrelated=req('set_exclusions',path=str(root),exclusions=['never-present']);after_unrelated=serialized();unrelated_status=req('status')['status']
  definite=req('set_exclusions',path=str(root),exclusions=['never-present','excluded.txt']);after_definite=serialized();definite_status=req('status')['status'];time.sleep(.7);durable_status=req('status')['status'];stopped_watches=watches()
  failed_resume=req('resume');time.sleep(.7);active_failure_status=req('status')['status'];active_failure_pages=serialized();time.sleep(.7);idle_failure_status=req('status')['status'];idle_failure_pages=serialized()
  if oldlimit:resource.prlimit(p.pid,resource.RLIMIT_NOFILE,oldlimit)
  parent.chmod(0o700)
  before_recovery=query();resume=req('resume');wait(lambda s:not s['busy']);after_recovery=serialized();recovery=query();recovery_status=req('status')['status'];time.sleep(.7);recovery_watches=watches()
  isbody=lambda r:any(x['path']==str(a) and x['text']==abody and x['exact'] for x in r.get('results',[]))
  same=lambda x:x['pages']==baseline['pages'] and x['revision']==baseline['revision']
  expected=[(str(a),abody)]
  result=dict(kind=kind,witness=witness,failed_resume=failed_resume,active_failure_status=active_failure_status,idle_failure_status=idle_failure_status,active_failure_pages=active_failure_pages,idle_failure_pages=idle_failure_pages,baseline_status=baseline_status,before_watches=before_watches,stopped_watches=stopped_watches,recovery_watches=recovery_watches,durable_status=durable_status,recovery_status=recovery_status,scan_status=scan_status,unrelated_status=unrelated_status,definite_status=definite_status,uncertain_query=uncertain_query,baseline=baseline,after_scan=after_scan,after_unrelated=after_unrelated,after_definite=after_definite,after_recovery=after_recovery,observations={
   'two_original_serialized_pages':len(baseline['pages'])==2,
   'baseline_scan_metrics_and_native_watches_positive':baseline_status['scan_entries']>0 and baseline_status['scan_directories']>0 and bool(before_watches),
   'explicit_scan_durable_matching_status':bool(scan_status['scan_error']) and scan_status['scan_error']==scan.get('error'),
   'unrelated_exclusion_durable_matching_status':bool(unrelated_status['scan_error']) and unrelated_status['scan_error']==unrelated.get('scan_error'),
   'definite_exclusion_durable_matching_status':bool(definite_status['scan_error']) and definite_status['scan_error']==definite.get('scan_error'),
   'unpaused_failed_resume_retains_diagnostic_and_idle_rows':failed_resume.get('accepted') is True and bool(failed_resume.get('scan_error')) and active_failure_status['scan_error']==idle_failure_status['scan_error']==failed_resume['scan_error'] and not active_failure_status['paused'] and not idle_failure_status['busy'] and active_failure_pages==idle_failure_pages==after_definite,
   'diagnostic_survives_service_sync':durable_status['scan_error']==definite.get('scan_error') and durable_status['ready'] and durable_status['error']=='',
   'failed_scan_stops_native_watch_coverage':not stopped_watches and not durable_status['busy'],
   'preflight_zero_enumeration_or_actual_directory_failure_metrics':(durable_status['scan_entries']==0 and durable_status['scan_directories']==0 and durable_status['scan_path_bytes']==0) if kind=='EMFILE' else (durable_status['scan_entries']==0 and durable_status['scan_directories']==1 and durable_status['scan_path_bytes']>0),
   'successful_recovery_clears_error_repopulates_metrics_watches':not recovery_status['scan_error'] and recovery_status['scan_entries']>0 and recovery_status['scan_directories']>0 and bool(recovery_watches),
   'initial_actual_body_query':isbody(positive),
   **({'uncertain_cached_source_filtered':uncertain_query.get('ok') is True and uncertain_query.get('results')==[]} if kind=='EACCES' else {}),
   'actual_scan_failed':scan.get('ok') is False,
   'unrelated_exclusion_accepted':unrelated.get('accepted') is True,
   'unrelated_exclusion_scan_failed':bool(unrelated.get('scan_error')),
   'scan_keeps_original_pages_revision':same(after_scan),
   'unrelated_exclusion_keeps_original_pages_revision':same(after_unrelated),
   'definite_exclusion_accepted':definite.get('accepted') is True,
   'definite_removes_only_matching_page_while_uncertain':after_definite['pages']==expected,
   'restored_body_before_rescan':isbody(before_recovery),
   'recovery_keeps_only_unexcluded_genuine_page':after_recovery['pages']==expected and isbody(recovery)})
  req('shutdown');c.close();c=None;p.wait(timeout=30)
 finally:
  if oldlimit and p.poll() is None:resource.prlimit(p.pid,resource.RLIMIT_NOFILE,oldlimit)
  parent.chmod(0o700)
  if c:c.close()
  if p.poll() is None:p.terminate();p.wait(timeout=30)
  log.close();result.update(events=events,closure=dict(service_exit=p.returncode,socket_absent=not sock.exists(),workers_absent=not any(Path(f'/proc/{pid}').exists() for pid in workers),known_workers=sorted(workers),permissions_restored=parent.stat().st_mode&0o777==0o700,private_limit_closed_with_process=not Path(f'/proc/{p.pid}').exists()))
  (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');cases.append(result)
for kind in ['EACCES','EMFILE']:run(kind)
result=dict(cases=cases,elapsed_ns=time.monotonic_ns()-begin,service_sha256=hashlib.sha256(B.read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),usage='token and billing usage unavailable; elapsed scope is this sequential helper only')
(O/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps([dict(kind=x['kind'],observations=x['observations'],closure=x['closure']) for x in cases]))
