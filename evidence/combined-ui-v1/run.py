#!/usr/bin/env python3
"""Native incomplete-index display from real production service limits; owned data only."""
import hashlib,json,os,socket,sqlite3,subprocess,time
from pathlib import Path
P=Path.cwd(); O=P/'local/build-own/combined-ui-v1';O.mkdir()
B=P/'local/build-integrated-v6/bin';probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe'
root=O/'corpus';root.mkdir();source=root/'source.txt';body='CACHE-UI-2203 genuine source retained when enumeration fails.';source.write_text(body)
data=O/'data';sock=O/'service.sock';log=(O/'service.log').open('wb')
service=subprocess.Popen([str(B/'kde-index-service'),'--data-dir',str(data),'--socket',str(sock),'--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','6'],stdout=log,stderr=log)
c=None;raw=[];buf=b'';rid=0;workers=set();app=None;result={}
def req(op,**kw):
 global rid,buf
 rid+=1;c.sendall(json.dumps(dict(id=str(rid),op=op,**kw)).encode()+b'\n')
 while b'\n' not in buf:
  chunk=c.recv(65536)
  if not chunk:raise RuntimeError('service closed')
  buf+=chunk
 line,buf=buf.split(b'\n',1);reply=json.loads(line);assert reply['id']==str(rid)
 raw.append(dict(clock_ns=time.monotonic_ns(),op=op,args=kw,reply=reply));return reply
def status_until(pred):
 for _ in range(900):
  s=req('status')['status']
  for f in Path(f'/proc/{service.pid}/task').glob('*/children'):
   try:workers.update(map(int,f.read_text().split()))
   except FileNotFoundError:pass
  if pred(s):return s
  time.sleep(.1)
 raise RuntimeError('status timeout')
try:
 for _ in range(500):
  try:c=socket.socket(socket.AF_UNIX);c.settimeout(120);c.connect(str(sock));break
  except OSError:c.close();c=None;time.sleep(.02)
 assert c is not None
 initial=status_until(lambda s:s['ready']);req('add_root',path=str(root));indexed=status_until(lambda s:not s['busy'] and s['indexed']==1)
 positive=req('search',text='CACHE-UI-2203')
 for i in range(10010):(root/f'bucket{i:05d}').mkdir()
 scan=req('scan');incomplete=status_until(lambda s:not s['busy'] and bool(s.get('scan_error')))
 preserved=req('search',text='CACHE-UI-2203')
 env=os.environ.copy()
 for k in ['HOME','XDG_CONFIG_HOME','XDG_DATA_HOME','XDG_CACHE_HOME']:
  d=O/k.lower();d.mkdir();env[k]=str(d)
 env.update(QT_QPA_PLATFORM='wayland',QT_QUICK_BACKEND='software')
 assert (Path(env['XDG_RUNTIME_DIR'])/env['WAYLAND_DISPLAY']).is_socket()
 with (O/'app.log').open('wb') as app_log:
  app=subprocess.Popen([str(B/'kde-index'),'--socket',str(sock),'--data-dir',str(data),'--models',str(P/'models'),'--service',str(B/'kde-index-service'),'--probe',str(probe),'--query','CACHE-UI-2203','--capture',str(O/'app.png'),'--capture-delay','8000'],env=env,stdout=app_log,stderr=app_log)
  app.wait(timeout=40)
 after_app=req('status')['status'];alive=service.poll() is None
 with sqlite3.connect(data/'index.sqlite') as db:pages=db.execute('select path,text from pages').fetchall()
 result=dict(initial=initial,indexed=indexed,scan=scan,incomplete=incomplete,after_app=after_app,pages=pages,observations=dict(baseline_genuine_body=any(r['path']==str(source) and r['text']==body and r['exact'] for r in positive['results']),production_directory_cap='directory limit exceeded' in incomplete['scan_error'],cached_genuine_body=any(r['path']==str(source) and r['text']==body and r['exact'] for r in preserved['results']),ready_despite_incomplete=incomplete['ready'],app_capture_succeeded=app.returncode==0 and (O/'app.png').exists(),external_service_survived_app_exit=alive))
 req('shutdown');c.close();c=None;service.wait(timeout=30)
finally:
 if app and app.poll() is None:app.terminate();app.wait(timeout=20)
 if c:c.close()
 if service.poll() is None:service.terminate();service.wait(timeout=30)
 log.close()
 result.update(raw=raw,binaries={n:hashlib.sha256((B/n).read_bytes()).hexdigest() for n in ['kde-index','kde-index-service']},sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [Path('src/core-v3/engine.cpp'),Path('src/service-v2/main.cpp'),Path('src/ui-v4/backend.cpp'),Path('src/ui-v4/Main.qml')]},closure=dict(service_exit=service.returncode,app_exit=app.returncode if app else None,socket_absent=not sock.exists(),known_workers=sorted(workers),workers_absent=not any(Path(f'/proc/{pid}').exists() for pid in workers)))
 (O/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result.get(k) for k in ['observations','closure']}))
