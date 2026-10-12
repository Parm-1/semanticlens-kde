#!/usr/bin/env python3
import os,json,hashlib,subprocess,time,sqlite3,argparse
from pathlib import Path
A=argparse.ArgumentParser();A.add_argument('--output',default='local/build-own/local-root-v1/repair-v2/caps');a=A.parse_args();P=Path.cwd();O=(P/a.output).resolve();O.mkdir();probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';binary=P/'local/build-own/local-root-v1/repair-v2/build/local-root-controls';checks=[];processes=[];workers=set();raw=[];begin=time.monotonic()
def check(name,value,detail=None):
 checks.append(dict(name=name,passed=bool(value),detail=detail))
 if not value:raise AssertionError(name)
def start(name,env):
 root=O/name;root.mkdir();(root/'source.txt').write_text('OWNED-CAP-99271 retained exact literal content')
 data=O/(name+'-data');log=(O/(name+'.log')).open('w');e=os.environ.copy();e.update(env)
 p=subprocess.Popen([str(binary),str(data),str(probe),str(P/'models/embeddinggemma-2-Q8_0.gguf'),str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),hashlib.sha256(probe.read_bytes()).hexdigest()],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True,env=e);processes.append((p,log));initial=json.loads(p.stdout.readline());raw.append(initial);workers.update(int(v) for f in Path(f'/proc/{p.pid}/task').glob('*/children') for v in f.read_text().split());check(name+'-ready',initial['status']['ready']);return p,root,data
 def_unused=0
def req(p,op,**kw):
 p.stdin.write(json.dumps(dict(op=op,**kw))+'\n');p.stdin.flush();r=json.loads(p.stdout.readline());raw.append(r);return r
def stop(p):p.stdin.write('{"op":"exit"}\n');p.stdin.flush();p.stdin.close();p.wait(timeout=20)
def rows(data):return sqlite3.connect(data/'index.sqlite').execute('select count(*) from pages').fetchone()[0]
try:
 for name,env,intervention,limit_name in [('entry',{'LOCAL_ROOT_ENTRY_LIMIT':'12'},lambda root:[(root/f'.ignored{i}').write_text('never eligible') for i in range(14)],'entry'),('directory',{'LOCAL_ROOT_DIRECTORY_LIMIT':'3'},lambda root:[(root/f'empty{i}').mkdir() for i in range(3)],'directory'),('watch',{'LOCAL_ROOT_WATCH_LIMIT':'2'},lambda root:(root/'new.txt').write_text('new source'),'watch coverage'),('path',{'LOCAL_ROOT_PATH_LIMIT':'1600'},lambda root:[(root/f'.ignored{i}').write_text('never eligible') for i in range(10)],'path storage')]:
  p,root,data=start(name,env);r=req(p,'add_root',path=str(root));check(name+'-ordinary-admitted',r['reply'].get('accepted'));req(p,'wait');check(name+'-source-content-indexed',rows(data)==1)
  before=req(p,'search',text='OWNED-CAP-99271');check(name+'-exact-literal-positive',before['reply']['results'][0]['exact'])
  intervention(root);r=req(p,'scan');check(name+'-limit-observable',limit_name in r['reply'].get('error','') and limit_name in r['status']['scan_error'],r)
  check(name+'-closed-scan-watches',not r['status']['busy'] and r['watches']==0);check(name+'-old-page-retained',rows(data)==1)
  rr=req(p,'search',text='OWNED-CAP-99271');check(name+'-retained-page-searchable',rr['reply']['results'][0]['exact']);stop(p)
 # Accepted cancellation must preserve actual rows and watches.
 p,root,data=start('cancel',{});req(p,'add_root',path=str(root));req(p,'wait');before=req(p,'status');r=req(p,'cancel_scan');check('cancel-accepted',r['cancel']['accepted']);check('cancel-rejects-precommit-scan',r['reply'].get('error')=='canceled');check('cancel-retains-actual-row-and-watches',rows(data)==1 and r['watches']==before['watches']);stop(p)
 outcome='pass'
except Exception as e:outcome='fail';failure=repr(e);raise
finally:
 for p,log in processes:
  if p.poll() is None:p.terminate()
  try:p.wait(timeout=20)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  log.close()
 (O/'results.json').write_text(json.dumps(dict(outcome=locals().get('outcome','fail'),error=locals().get('failure'),checks=checks,raw=raw,wall_seconds=time.monotonic()-begin,process_closure=all(p.poll() is not None for p,_ in processes),known_workers=sorted(workers),worker_closure=not any(Path(f'/proc/{pid}').exists() for pid in workers)),indent=2)+'\n')
