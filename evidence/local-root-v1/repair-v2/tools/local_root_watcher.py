#!/usr/bin/env python3
import hashlib,json,os,socket,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/build-own/local-root-v1/repair-v2/watcher-control';O.mkdir();probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';records=[]
for name,binary in [('candidate',P/'local/build-own/local-root-v1/repair-v2/build/local-root-service'),('accepted',P/'local/installed-host-v3/stage/usr/bin/kde-index-service')]:
 root=O/name;root.mkdir();(root/'source.txt').write_text('OWNED-WATCHER-19824 source')
 data=O/(name+'-data');sock=str(O/(name+'.socket'));buf=b'';rid=0
 log=(O/(name+'.log')).open('wb');p=subprocess.Popen([str(binary),'--data-dir',str(data),'--socket',sock,'--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')],stdout=log,stderr=log)
 def req(op,**kw):
  global rid,buf
  rid+=1;c.sendall(json.dumps(dict(op=op,id=str(rid),**kw)).encode()+b'\n')
  while b'\n' not in buf:buf+=c.recv(65536)
  line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==str(rid);return r
 def wait(pred):
  for i in range(500):
   r=req('status')['status']
   if pred(r):return r
   time.sleep(.03)
  return r
 try:
  for i in range(200):
   try:c=socket.socket(socket.AF_UNIX);c.connect(sock);c.settimeout(60);break
   except OSError:time.sleep(.02)
  wait(lambda s:s['ready']);req('add_root',path=str(root));wait(lambda s:not s['busy']);time.sleep(2)
  fds={f.name:f.read_text() for f in Path(f'/proc/{p.pid}/fdinfo').iterdir() if 'inotify' in f.read_text()}
  (root/'auto.txt').write_text('OWNED-AUTOMATIC-77312 file');s=wait(lambda s:s['indexed']==2)
  records.append({'name':name,'status':s,'inotify_before':fds,'root_inode':root.stat().st_ino})
  req('shutdown');c.close();p.wait(timeout=20)
 finally:
  if p.poll() is None:p.terminate()
  p.wait(timeout=20);log.close()
(O/'results.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(records))
