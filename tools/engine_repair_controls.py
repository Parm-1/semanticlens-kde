#!/usr/bin/env python3
"""Actual worker falsifiers for linked engine-v2; only disposable local/engine-* data."""
import argparse,hashlib,json,os,socket,sqlite3,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--project-root');p.add_argument('--service',default='build-engine/kde-index-service');p.add_argument('--legacy-service',default='evidence/engine-v1/candidate/bin/kde-index-service');a=p.parse_args();P=Path(a.project_root).resolve() if a.project_root else Path(__file__).resolve().parents[1];out=P/a.output
if out.exists():raise SystemExit('preserve existing receipt')
out.mkdir(parents=True);root=out/'corpus';root.mkdir();data=out/'data';link=out/'model.gguf';link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf');wrong=out/'wrong.gguf';wrong.write_bytes(b'wrong');sock='/tmp/engine-repair-'+str(os.getpid());buf=b'';counter=0;procs=[];workers=set();responses=[];checks=[];begin=time.monotonic()
def check(n,v,d=None):
 checks.append({'name':n,'pass':bool(v),'raw':d})
 if not v:raise AssertionError(n+': '+str(d))
def send(op,**kw):
 global counter
 counter+=1;i=str(counter);conn.sendall(json.dumps(dict(id=i,op=op,**kw)).encode()+b'\n');return i
def recv():
 global buf
 while b'\n' not in buf:buf+=conn.recv(65536)
 line,buf=buf.split(b'\n',1);r=json.loads(line);responses.append(r);return r
def req(op,**kw):
 i=send(op,**kw);r=recv();assert r['id']==i,(i,r);return r
def status():return req('status')['status']
def children():
 c={int(v) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in f.read_text().split()};workers.update(c);return c
def wait(pred,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=status();children()
  if pred(s):return s
  time.sleep(.04)
 raise AssertionError('timeout '+str(s))
def start(binary=None):
 global conn,proc,buf
 buf=b'';log=(out/f'service-{len(procs)}.log').open('wb');proc=subprocess.Popen([str(binary or P/a.service),'--socket',sock,'--data-dir',str(data),'--probe',str(P/'build-probe/embedding-probe'),'--probe-sha256',hashlib.sha256((P/'build-probe/embedding-probe').read_bytes()).hexdigest(),'--text-model',str(link),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')],stdout=log,stderr=log);procs.append((proc,log));end=time.monotonic()+10
 while time.monotonic()<end:
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(sock);conn.settimeout(120);return
  except OSError:time.sleep(.04)
 raise AssertionError('no socket')
def stop():
 req('shutdown');conn.close();proc.wait(timeout=15)
def dbrows(sql,args=()):
 with sqlite3.connect(data/'index.sqlite') as db:return db.execute(sql,args).fetchall()
def redirect(path):link.unlink();link.symlink_to(path)
def idle_worker():
 wait(lambda s:not s['busy']);end=time.monotonic()+13
 # actual producer exit, independent of cheap status timer
 while time.monotonic()<end:
  if not children():return
  time.sleep(.1)
 raise AssertionError('worker did not leave')
try:
 for i in range(5):(root/f'a{i}-near.txt').write_text('Tracking serial ZXQ 7491 belongs to a cooling appliance.')
 (root/'z-literal.txt').write_text('The serial ZXQ-7491 identifies this maintenance item.')
 (root/'boundary.txt').write_text('a '*898+' ZXQ-7491 genuine source passage beyond the segmentation edge.')
 start();wait(lambda s:s['ready']);check('valid-add-root',req('add_root',path=str(root))['ok']);wait(lambda s:not s['busy'])
 r=req('search',text='ZXQ-7491',limit=5);rr=r['results'];check('R1-literal-precedes-five-token-near',all(x['exact'] for x in rr[:2]) and {Path(x['path']).name for x in rr[:2]}=={'z-literal.txt','boundary.txt'},rr)
 check('R1-token-tier-is-not-exact',any(not x['exact'] and x['term_match'] and x['match_type']=='terms' and x['score']<1 for x in rr[2:]),rr)
 b=next(x for x in rr if Path(x['path']).name=='boundary.txt');check('R2-boundary-genuine-literal-passage',b['exact'] and 'ZXQ-7491' in b['text'] and b['page']==0 and b['kind']=='text',b)
 check('versioned-full-pages-extractor-identity',r['identity']['representation']==3 and set(r['identity']['extractors'])=={'pdftotext','pdfinfo','pdftoppm','tesseract'},r['identity'])
 long='LONG '+('source phrase '*80);(root/'long.txt').write_text('prefix '+long+' suffix');req('scan');wait(lambda s:not s['busy']);r=req('search',text=long.strip());check('R2-long-query-bounded-genuine-snippet',r['ok'] and r['results'][0]['exact'] and long.strip() in r['results'][0]['text'] and len(r['results'][0]['text'])<=2448,r)
 # A canceled pause queued during actual inference has no state, persisted pause, or scan lifetime effect.
 (root/'aaa-active.txt').write_text('Delivery route maintenance content. '*800)
 req('scan');wait(lambda s:s['busy'] and s['current'].endswith('aaa-active.txt'));time.sleep(.03)
 pause=send('pause');cancel=send('cancel',target=pause);got={}
 while len(got)<2:
  r=recv();got[r['id']]=r
 check('R4-accepted-cancel-rejects-pause',got[cancel]['accepted'] and not got[pause]['ok'] and got[pause]['error']=='canceled',got)
 s=status();check('R4-canceled-pause-no-live-or-persisted-change',not s['paused'] and dbrows("select value from meta where key='paused'")!=[('1',)],s)
 r=req('set_exclusions',path=str(root),exclusions=['../outside']);check('R4-invalid-exclusion-rejected',not r['ok'],r)
 wait(lambda s:not s['busy']);check('R4-invalid-and-canceled-controls-preserve-complete-active-scan',dbrows('select count(*) from segments where path=?',(str(root/'aaa-active.txt'),))[0][0]>1)
 (root/'aaa-active.txt').write_text('Changed delivery route maintenance content. '*900);req('scan');wait(lambda s:s['busy'] and s['current'].endswith('aaa-active.txt'));time.sleep(.03);r=req('pause');check('R4-valid-pause-accepted',r['ok'] and r['accepted'],r);check('R4-valid-pause-no-partial-commit',status()['paused'] and dbrows('select count(*) from segments where path=?',(str(root/'aaa-active.txt'),))[0][0]==0)
 r=req('cancel',target=r['id']);check('R4-late-cancel-not-accepted',not r['accepted'],r);check('R4-valid-resume-completes',req('resume')['ok']);wait(lambda s:not s['busy']);check('R4-resume-full-file',dbrows('select count(*) from segments where path=?',(str(root/'aaa-active.txt'),))[0][0]>1)
 idle_worker();old=dbrows('select count(*) from segments')[0][0];redirect(wrong);r=req('search',text='cooling appliance');s=status();check('R3-query-reload-model-rejection-visible-closed',not r['ok'] and not s['ready'] and not s['model_ready'] and 'byte size mismatch' in s['error'] and not children(),{'response':r,'status':s});check('R3-no-fabricated-vectors',dbrows('select count(*) from segments')[0][0]==old)
 redirect(P/'models/embeddinggemma-2-Q8_0.gguf');check('R3-deliberate-retry',req('resume')['ok']);wait(lambda s:s['ready'] and not s['busy']);idle_worker();redirect(wrong);(root/'new-failure.txt').write_text('Uncommitted new content');req('scan');s=wait(lambda s:bool(s['error']));check('R3-index-reload-rejection-visible-closed',not s['ready'] and not s['model_ready'] and not children(),s);check('R3-index-no-fabricated-row',not dbrows('select path from files where path=?',(str(root/'new-failure.txt'),)))
 # Wipe after model rejection removes roots/content even when fresh verification fails.
 revision=s['index_revision'];r=req('wipe');s=status();check('R5-wipe-removal-independent-of-verification',r['ok'] and r['accepted'] and r['removed'] and not r['ready'] and s['roots']==[] and s['segments']==0 and not s['ready'] and s['index_revision']>revision,{'response':r,'status':s});check('R5-failed-wipe-no-old-database',not (data/'index.sqlite').exists())
 redirect(P/'models/embeddinggemma-2-Q8_0.gguf');r=req('wipe');check('R5-explicit-wipe-rebuild',r['ok'] and r['ready'] and r['removed'],r);check('R5-rebuilt-fresh-empty',status()['roots']==[] and status()['segments']==0);req('add_root',path=str(root));wait(lambda s:not s['busy']);stop()
 # Contradict persisted representation/extractor identity, retaining old content for closed startup.
 with sqlite3.connect(data/'index.sqlite') as db:
  identity=json.loads(db.execute("select value from meta where key='identity'").fetchone()[0]);identity['extractors']['pdftotext']='contradictory extractor';db.execute("update meta set value=? where key='identity'",(json.dumps(identity,separators=(',',':')),));db.commit()
 start();s=wait(lambda s:bool(s['error']));check('R5-incompatible-startup-fails-closed',not s['ready'] and 'stored identity mismatch' in s['error'],s);check('R5-fatal-startup-immediate-worker-closure',not children(),sorted(workers));t=time.monotonic();s=status();check('R5-fatal-status-cheap',time.monotonic()-t<.25);check('R5-incompatible-old-content-retained-until-wipe',dbrows('select count(*) from segments')[0][0]>0);r=req('wipe');check('R5-incompatible-explicit-wipe-recovery',r['ok'] and r['ready'] and r['removed'] and status()['roots']==[],r);stop()
 # Actual sealed v1 executable produces a legacy index; v2 cannot silently adopt its vectors.
 data=out/'legacy-data';legacy=out/'legacy-corpus';legacy.mkdir();(legacy/'source.txt').write_text('Legacy serial LEGACY-8821 source content')
 start(P/a.legacy_service);wait(lambda s:s['ready']);req('add_root',path=str(legacy));wait(lambda s:not s['busy']);check('R2-actual-v1-index-has-vector',dbrows('select count(*) from segments')[0][0]==1);stop()
 start();s=wait(lambda s:bool(s['error']));check('R2-actual-v1-representation-rejected',not s['ready'] and 'stored identity mismatch' in s['error'] and dbrows('select count(*) from segments')[0][0]==1,s);check('R2-legacy-explicit-wipe-rebuild',req('wipe')['ready'] and status()['segments']==0);stop();outcome='pass'
except Exception as error:
 outcome='fail';failure=repr(error);raise
finally:
 for proc,log in procs:
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 try:conn.close()
 except Exception:pass
 if Path(sock).exists():Path(sock).unlink()
 def sanitize(x):
  if isinstance(x,str):return x.replace(str(out),'<fixture>').replace(str(P),'<project>')
  if isinstance(x,list):return [sanitize(v) for v in x]
  if isinstance(x,dict):return {k:sanitize(v) for k,v in x.items()}
  return x
 result={'outcome':locals().get('outcome','fail'),'error':locals().get('failure'),'checks':checks,'responses':responses,'wall_seconds':time.monotonic()-begin,'resource_closure':{'services_closed':all(p.poll() is not None for p,_ in procs),'known_workers_closed':not any(Path(f'/proc/{pid}').exists() for pid in workers),'socket_absent':not Path(sock).exists()},'measurement_scope':'actual CPU diagnostic integration and falsifiers, monotonic whole runner wall time; no frozen benchmark retrieval or tuning','package_usage':'agent token/billing metrics unavailable'}
 (out/'results.json').write_text(json.dumps(sanitize(result),indent=2)+'\n');print(json.dumps({'outcome':result['outcome'],'checks':len(checks),'output':str(out/'results.json')}))
