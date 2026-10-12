#!/usr/bin/env python3
"""Actual permission uncertainty retains a prior index across closed restart."""
import hashlib,json,os,sqlite3,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/build-own/local-root-v1/repair-v2/permissions';O.mkdir();parent=O/'parent';parent.mkdir();root=parent/'corpus';root.mkdir();source=root/'source.txt';source.write_text('PERMISSION-RETAINED-76281 genuine old source body');data=O/'data';probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';binary=P/'local/build-own/local-root-v1/repair-v2/build/local-root-controls';workers=set();records=[];processes=[];begin=time.monotonic()
def run(name,commands):
 log=(O/(name+'.log')).open('w');p=subprocess.Popen([str(binary),str(data),str(probe),str(P/'models/embeddinggemma-2-Q8_0.gguf'),str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),hashlib.sha256(probe.read_bytes()).hexdigest()],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True);processes.append(p);initial=json.loads(p.stdout.readline());records.append(initial);workers.update(int(v) for f in Path(f'/proc/{p.pid}/task').glob('*/children') for v in f.read_text().split())
 if initial['status']['ready']:
  for request in commands:
   p.stdin.write(json.dumps(request)+'\n');p.stdin.flush();records.append(json.loads(p.stdout.readline()))
  p.stdin.write('{"op":"exit"}\n');p.stdin.flush();p.stdin.close()
 p.wait(timeout=30);log.close();return initial,p.returncode
try:
 first,rc=run('positive',[{'op':'add_root','path':str(root)},{'op':'wait'},{'op':'search','text':'PERMISSION-RETAINED-76281'}]);positive=records[-1]['reply']['results'][0]['exact'];parent.chmod(0);closed,closed_rc=run('closed',[]);db=sqlite3.connect(data/'index.sqlite');retained=db.execute('select path,text from pages').fetchall();db.close();parent.chmod(0o700);restored,rrc=run('recovery',[{'op':'wait'},{'op':'search','text':'PERMISSION-RETAINED-76281'}]);checks={'actual-old-source-positive':positive,'permission-restart-closed':closed_rc==3 and not closed['status']['ready'] and 'Permission denied' in closed['status']['error'],'serialized-old-page-retained-while-closed':retained==[(str(source),source.read_text())],'restored-permission-restart-genuine-literal-positive':rrc==0 and records[-1]['reply']['results'][0]['exact']};outcome='pass' if all(checks.values()) else 'fail'
finally:
 parent.chmod(0o700)
 for p in processes:
  if p.poll() is None:p.terminate();p.wait(timeout=20)
 result={'outcome':locals().get('outcome','fail'),'checks':locals().get('checks',{}),'records':records,'retained_pages':locals().get('retained',[]),'wall_seconds':time.monotonic()-begin,'closure':{'processes_closed':all(p.poll() is not None for p in processes),'known_worker_pids':sorted(workers),'workers_closed':not any(Path(f'/proc/{pid}').exists() for pid in workers),'source_permission_restored':parent.stat().st_mode&0o777==0o700}};(O/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'outcome':result['outcome'],'checks':len(result['checks']),'closure':result['closure']}))
