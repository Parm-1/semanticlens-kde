#!/usr/bin/env python3
import hashlib,json,socket,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/build-own/local-root-v1/repair-v2/watch-gap';O.mkdir();root=O/'corpus';root.mkdir();(root/'source.txt').write_text('ORIGINAL-OWNED-92111 initial source content');data=O/'data';sock=str(O/'service.socket');probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';log=(O/'service.log').open('wb');p=subprocess.Popen([str(P/'local/build-own/local-root-v1/repair-v2/build/local-root-service'),'--data-dir',str(data),'--socket',sock,'--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','2'],stdout=log,stderr=log);buf=b'';rid=0;records=[];workers=set()
def req(op,**kw):
 global rid,buf
 rid+=1;c.sendall(json.dumps(dict(op=op,id=str(rid),**kw)).encode()+b'\n')
 while b'\n' not in buf:buf+=c.recv(65536)
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==str(rid);records.append({'clock_ns':time.monotonic_ns(),'op':op,'reply':r});return r
inode=root.stat().st_ino
def witness():
 stamp=time.monotonic_ns();raw={f.name:f.read_text() for f in Path(f'/proc/{p.pid}/fdinfo').iterdir() if 'inotify' in f.read_text()};present=any(f'ino:{inode:x} ' in v for v in raw.values());return {'clock_ns':stamp,'root_inode':inode,'root_watch_installed':present,'raw':raw}
try:
 for i in range(200):
  try:c=socket.socket(socket.AF_UNIX);c.connect(sock);c.settimeout(30);break
  except OSError:time.sleep(.02)
 for i in range(1500):
  s=req('status')['status'];workers.update(int(v) for f in Path(f'/proc/{p.pid}/task').glob('*/children') for v in f.read_text().split())
  if s['ready']:break
  time.sleep(.02)
 req('add_root',path=str(root));before=witness();create_before=time.monotonic_ns();(root/'auto.txt').write_text('WATCH-GAP-55328 new body before watch installation');create_after=time.monotonic_ns();time.sleep(3);after=witness();s=req('status')['status'];query=req('search',text='WATCH-GAP-55328');req('scan')
 for i in range(500):
  final=req('status')['status']
  if not final['busy']:break
  time.sleep(.02)
 positive=req('search',text='WATCH-GAP-55328');result={'outcome':'observed-gap' if not before['root_watch_installed'] and after['root_watch_installed'] and s['indexed']==1 and positive['results'][0]['exact'] else 'no-gap-qualified','before':before,'creation_clock_ns':[create_before,create_after],'after':after,'pre_scan_status':s,'pre_scan_query':query,'explicit_scan_positive':positive};req('shutdown');c.close();p.wait(timeout=20)
finally:
 if p.poll() is None:p.terminate()
 p.wait(timeout=20);log.close();result=locals().get('result',{'outcome':'failed'});result['responses']=records;result['closure']={'service_closed':p.poll() is not None,'workers_closed':not any(Path(f'/proc/{pid}').exists() for pid in workers),'socket_absent':not Path(sock).exists()};(O/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'outcome':result['outcome'],'before':result.get('before',{}).get('root_watch_installed'),'after':result.get('after',{}).get('root_watch_installed')}))
