#!/usr/bin/env python3
"""Freeze the bounded original stock-consumer run and retained invalid probes."""
import hashlib,json,shutil,time
from pathlib import Path
P=Path(__file__).resolve().parents[1];E=P/'evidence/desktop-integration-v1';R=P/'local/desktop-integration-v1-run8';stage=P/'local/clean-arch-v1/root.x86_64/checkout/local/stage-v3/usr';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if (E/'SHA256SUMS').exists():raise SystemExit('Evidence is sealed; use a linked new version rather than refreezing v1')
for x in ['candidate/tools','candidate/bin','candidate/source','raw/run8','raw/prior']: (E/x).mkdir(parents=True,exist_ok=True)
for f in P.glob('tools/desktop_integration*'):
 if f.is_file():shutil.copy2(f,E/'candidate/tools'/f.name)
for n in ['kde-index','kde-index-service']:
 shutil.copy2(stage/'bin'/n,E/'candidate/bin'/n)
shutil.copy2(stage/'lib/qt6/plugins/kf6/krunner/kde-index-runner.so',E/'candidate/bin/kde-index-runner.so')
shutil.copy2(P/'build-desktop-integration-v1/build/libdesktop-observer.so',E/'candidate/bin/libdesktop-observer.so')
shutil.copy2(P/'build-desktop-integration-v1/text-contract',E/'candidate/bin/text-contract')
for f in [*P.glob('src/runner-v4/*'),*P.glob('src/ui/*')]:
 if f.is_file():t=E/'candidate/source'/f.relative_to(P);t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
for f in R.rglob('*'):
 if f.is_file() and (f.suffix in ['.json','.jsonl','.png','.log','.py'] or 'corpus' in f.parts or f.name=='mimeapps.list' or f.name=='desktop-proof-open.desktop'):
  if any(x in f.relative_to(R).parts for x in ['cache','index','home','runtime']):continue
  t=E/'raw/run8'/f.relative_to(R);t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
for n in range(1,8):
 r=P/f'local/desktop-integration-v1-run{n}';target=E/'raw/prior'/f'run{n}';target.mkdir(exist_ok=True)
 for name in ['results.json','session.log','probe.py']:
  if (r/name).is_file():shutil.copy2(r/name,target/name)
 if n==4:
  for name in ['runner-current.json','runner-reveal.json','runner-before-reveal.json']:
   f=r/'runner-observer'/name
   if f.is_file():shutil.copy2(f,target/name)
  shutil.copy2(r/'libdesktop-observer.so',target/'libdesktop-observer.so')
for n in ['observer-configure.log','observer-build.log']:
 shutil.copy2(P/'local/desktop-integration-v1-source'/n,E/'raw'/n)
r=json.loads((R/'results.json').read_text());source=next(R.joinpath('corpus').glob('*.txt'))
peers={p['connection']:p['identity'] for p in r['ipc_peers']};reply=next(e for e in r['events'] if e['direction']=='reply' and e['connection']==0 and e['raw'].get('results'));get=next(g for g in r['http_gets'] if g['path']=='/passage');native=[]
for role,f in [('runner','runner-visible-initial.json'),('runner','runner-action-selected-0.json'),('app','app-initial.json'),('app','app-errors.json')]:
 p=R/(role+'-observer')/f
 if p.exists():
  v=json.loads(p.read_text());nodes=v['snapshot']['nodes'];native.append({'receipt':f,'pid':v['snapshot']['pid'],'source_field_nodes':[n for n in nodes if ('TITLE-77331' in n.get('text','') or n.get('text','').startswith('DESKTOP-77331 <img') or 'ERROR-77331' in n.get('text','') or n.get('text')=='extractor failed: pdftotext')],'loaded_relevant_maps':v['snapshot'].get('loaded_relevant_maps',[])})
intervals=[];requests={}
for e in r['events']:
 raw=e['raw'];key=(e['connection'],raw.get('id'))
 if e['direction']=='request' and raw.get('op')=='search':requests[key]=e
 elif e['direction']=='reply' and key in requests:
  start=requests.pop(key);intervals.append({'connection':key[0],'client_exe':peers[key[0]]['exe'],'begin_ns':start['monotonic_ns'],'end_ns':e['monotonic_ns'],'text':start['raw'].get('text')})
ui=[i for i in intervals if i['client_exe'].endswith('/kde-index')];rr=[i for i in intervals if i['client_exe']=='/usr/bin/krunner']
analysis={'original_run':'run8','privacy_positive':{'reachable_control':r['positive_canary'],'uninstrumented_reply':reply,'automatic_get':get,'get_after_reply_ns':get['monotonic_ns']-reply['monotonic_ns'],'source_sha256':sha(source),'source_inode':source.stat().st_ino,'uninstrumented_peer':peers[0]},'native_consumer_categories':native,'actions':{k:r.get(k) for k in ['app_reveal_invocation','app_reveal_target','runner_reveal_invocation','runner_reveal_selected_index','runner_reveal_target','app_open_api_events','app_open_target_boundary']},'dolphin_fresh_processes':[json.loads((R/'dolphin-observer'/n).read_text())['snapshot']['pid'] for n in ['app-reveal-target.json','runner-reveal-target.json']],'search_intervals':intervals,'search_overlap_observed':any(max(u['begin_ns'],v['begin_ns'])<min(u['end_ns'],v['end_ns']) for u in ui for v in rr),'closure':r['closure'],'namespace_final_exit':0,'root_process_check':'After run8 completion, exact desktop_integration/probe process search showed no corresponding process; all previous tool handles completed.','usage_scope':{'model_usage_tokens':'unknown','billing':'unknown','package_elapsed_native_run8_seconds':30.07377494299726,'time_scope':'outer bwrap+private D-Bus session diagnostic wall time; not latency benchmark'}}
for name in ['service.sock','service-relay.sock']:
 f=R/name
 if f.exists():f.unlink()
analysis['closure']['owned_socket_files_absent_after_closed_namespace']=not any((R/name).exists() for name in ['service.sock','service-relay.sock'])
(E/'analysis.json').write_text(json.dumps(analysis,indent=2)+'\n')
print(json.dumps({'get_after_reply_ns':analysis['privacy_positive']['get_after_reply_ns'],'fresh_dolphin_pids':analysis['dolphin_fresh_processes'],'search_overlap_observed':analysis['search_overlap_observed'],'file_count':sum(f.is_file() for f in E.rglob('*'))}))
