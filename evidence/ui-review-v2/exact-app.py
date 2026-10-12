#!/usr/bin/env python3
"""Native exact-GUI privacy control using a new selected synthetic root and loopback."""
from pathlib import Path
import socket,threading,time,subprocess,json,hashlib,sys,os
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v2';own=root/'local/ui-review-v2-app';own.mkdir(parents=True,exist_ok=True)
candidate=root/'evidence/ui-v2/candidate';models=root/'models';endpoint=own/'service.sock';events=[];stop=threading.Event();http=socket.socket();http.bind(('127.0.0.1',0));http.listen();http.settimeout(.2);port=http.getsockname()[1]
def listener():
 while not stop.is_set():
  try:c,addr=http.accept()
  except socket.timeout:continue
  except OSError:return
  with c:
   c.settimeout(1)
   try:raw=c.recv(4096);events.append({'event':'http-request','raw':raw.decode('latin1')});c.sendall(b'HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
   except OSError:pass
thread=threading.Thread(target=listener);thread.start();corpus=own/'corpus';corpus.mkdir(exist_ok=True)
source=f"<b>EXACT-APP-8421</b><img src='http://127.0.0.1:{port}/exact-app-canary.png'>"
(corpus/'markup.txt').write_text(source);(e/'exact-app-input.txt').write_text(source)
service=probe=None;sequence=0
rawlog=(e/'exact-app-service.log').open('w')
def request(op,**args):
 global sequence
 sequence+=1;row={'id':f'app-review-{sequence}','op':op,**args};events.append({'event':'request',**row})
 with socket.socket(socket.AF_UNIX) as c:
  c.settimeout(45);c.connect(str(endpoint));c.sendall(json.dumps(row).encode()+b'\n');buf=b''
  while b'\n' not in buf:
   chunk=c.recv(1<<20)
   if not chunk:raise RuntimeError('EOF before reply')
   buf+=chunk
  out=json.loads(buf.split(b'\n')[0]);events.append({'event':'reply',**out});return out
def until(predicate,seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if predicate():return True
  time.sleep(.05)
 return False
result={}
try:
 sha=hashlib.sha256((candidate/'bin/embedding-probe').read_bytes()).hexdigest()
 service=subprocess.Popen([str(candidate/'bin/kde-index-service'),'--data-dir',str(own/'data'),'--socket',str(endpoint),'--probe',str(candidate/'bin/embedding-probe'),'--probe-sha256',sha,'--text-model',str(models/'embeddinggemma-2-Q8_0.gguf'),'--image-model',str(models/'mmproj-embeddinggemma-2-Q8_0.gguf')],stdout=rawlog,stderr=rawlog)
 events.append({'event':'service-start','pid':service.pid})
 assert until(lambda:endpoint.exists(),10)
 assert until(lambda:request('status')['status']['ready'],30)
 assert request('add_root',path=str(corpus))['accepted']
 assert until(lambda:request('status')['status']['indexed']==1 and not request('status')['status']['busy'],30)
 actual=request('search',text='EXACT-APP-8421',kind='',limit=20);assert actual['results'][0]['text']==source
 events.append({'event':'before-GUI','http_requests':sum(x['event']=='http-request' for x in events)})
 t=time.monotonic()
 with (e/'exact-app.log').open('w') as out:
  gui=subprocess.run([str(candidate/'bin/kde-index'),'--socket',str(endpoint),'--data-dir',str(own/'data'),'--models',str(models),'--service',str(candidate/'bin/kde-index-service'),'--probe',str(candidate/'bin/embedding-probe'),'--query','EXACT-APP-8421','--capture',str(e/'exact-app.png'),'--capture-delay','3500'],stdout=out,stderr=out,timeout=20)
 result={'gui_exit_code':gui.returncode,'GUI_subprocess_wall_seconds':time.monotonic()-t,'http_requests':sum(x['event']=='http-request' for x in events),'capture_exists':(e/'exact-app.png').exists(),'scope':'exact staged GUI on native desktop with preindexed selected synthetic source; not a latency or inference proof'}
 assert gui.returncode==0 and result['http_requests']==0 and result['capture_exists']
finally:
 if service and service.poll() is None:
  try:request('shutdown')
  except (OSError,RuntimeError):service.terminate()
  try:service.wait(timeout=15)
  except subprocess.TimeoutExpired:service.kill();service.wait(timeout=5)
 stop.set();http.close();thread.join(timeout=2);rawlog.close()
 result['service_exit_code']=service.returncode if service else None;result['socket_exists']=endpoint.exists();result['listener_thread_alive']=thread.is_alive()
 result['events']=events;(e/'exact-app.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='events'}))
