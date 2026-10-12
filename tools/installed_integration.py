#!/usr/bin/env python3
"""Owned staged Wayland/RunnerManager integration; disposable synthetic roots only."""
import argparse, hashlib, json, os, select, shutil, socket, subprocess, threading, time
from pathlib import Path
from retrieval_service import descendants

p=argparse.ArgumentParser(description=__doc__)
for flag in ('stage','models','runner-check','output'):p.add_argument('--'+flag,type=Path,required=True)
a=p.parse_args();stage,models,check,out=[v.resolve() for v in (a.stage,a.models,a.runner_check,a.output)]
if out.exists():p.error('use a fresh disposable output')
out.mkdir(parents=True);corpus=out/'corpus';corpus.mkdir()
root=Path(__file__).resolve().parents[1]
for name in ('manual.pdf','payment.txt','picture.jpg'):shutil.copy2(root/'evidence/ui-v2/inputs'/name,corpus/name)
for name in ('config','cache','xdg-data'):(out/name).mkdir()
env={**os.environ,'XDG_CONFIG_HOME':str(out/'config'),'XDG_CACHE_HOME':str(out/'cache'),'XDG_DATA_HOME':str(out/'xdg-data'),'QT_FORCE_STDERR_LOGGING':'1'}
back=f'/tmp/kde-index-installed-service-{os.getpid()}';front=f'/tmp/kde-index-installed-proxy-{os.getpid()}'
probe=stage/'bin/embedding-probe';service=stage/'bin/kde-index-service';data=out/'data'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
report={'stage_binary_sha256':{f.name:sha(f) for f in (probe,service,stage/'bin/kde-index')},'scope':'synthetic staged host Wayland and native RunnerManager discovery; diagnostic timing only','events':[]}
log=(out/'service.log').open('wb');proc=subprocess.Popen([str(service),'--data-dir',str(data),'--socket',back,'--probe',str(probe),'--probe-sha256',sha(probe),'--text-model',str(models/'embeddinggemma-2-Q8_0.gguf'),'--image-model',str(models/'mmproj-embeddinggemma-2-Q8_0.gguf')],stdout=log,stderr=log,env=env)
stop=threading.Event();threads=[];peers=[];lock=threading.Lock();known={proc.pid};app=runner=None;client=None;listener=None;sequence=0;buffer=b''
def req(op,**fields):
 global sequence,buffer
 sequence+=1;identity=f'parent-{sequence}';client.sendall(json.dumps({'id':identity,'op':op,**fields}).encode()+b'\n')
 while b'\n' not in buffer:
  chunk=client.recv(65536)
  if not chunk:raise RuntimeError('service EOF')
  buffer+=chunk
 line,buffer=buffer.split(b'\n',1);r=json.loads(line)
 if r.get('id')!=identity or not r.get('ok'):raise RuntimeError(f'bad parent reply: {r}')
 return r

def relay(peer,index):
 destination=socket.socket(socket.AF_UNIX);destination.connect(back);streams={peer:b'',destination:b''}
 peers.extend((peer,destination))
 try:
  while not stop.is_set():
   ready,_,_=select.select([peer,destination],[],[],.1)
   for source in ready:
    chunk=source.recv(65536)
    if not chunk:return
    streams[source]+=chunk
    if len(streams[source])>4*1024*1024:raise RuntimeError('bounded relay frame exceeded')
    while b'\n' in streams[source]:
     line,streams[source]=streams[source].split(b'\n',1)
     with lock:report['events'].append({'client':index,'direction':'request' if source is peer else 'reply','monotonic_ns':time.monotonic_ns(),'raw':json.loads(line)})
    (destination if source is peer else peer).sendall(chunk)
 except (OSError,ValueError) as e:
  with lock:report.setdefault('relay_errors',[]).append(str(e))
 finally:peer.close();destination.close()

def accept():
 index=0
 while not stop.is_set():
  try:peer,_=listener.accept()
  except socket.timeout:continue
  except OSError:return
  t=threading.Thread(target=relay,args=(peer,index));index+=1;threads.append(t);t.start()
try:
 deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  if proc.poll() is not None:raise RuntimeError('service startup exit')
  client=socket.socket(socket.AF_UNIX)
  try:client.connect(back);client.settimeout(120);break
  except OSError:client.close();client=None;time.sleep(.05)
 if client is None:raise TimeoutError('service socket')
 deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  status=req('status')['status']
  if status['ready']:break
  if status['error']:raise RuntimeError(status['error'])
  time.sleep(.1)
 else:raise TimeoutError('initial readiness')
 req('add_root',path=str(corpus));deadline=time.monotonic()+120
 while time.monotonic()<deadline:
  status=req('status')['status'];known.update(descendants(proc.pid))
  if status['indexed']==3 and not status['busy']:break
  if status['error']:raise RuntimeError(status['error'])
  time.sleep(.1)
 else:raise TimeoutError('indexing')
 report['indexed_status']=status
 before=descendants(proc.pid);report['producer_present_before_idle']=len(before)>1
 deadline=time.monotonic()+20
 while descendants(proc.pid)!={proc.pid}:
  if time.monotonic()>deadline:raise TimeoutError('producer idle closure')
  time.sleep(.1)
 report['pre_client_process_inventory']={'monotonic_ns':time.monotonic_ns(),'pids':sorted(descendants(proc.pid))}
 listener=socket.socket(socket.AF_UNIX);listener.bind(front);listener.listen();listener.settimeout(.1)
 accept_thread=threading.Thread(target=accept);accept_thread.start()
 with (out/'app.log').open('wb') as app_log,(out/'runner.log').open('wb') as runner_log,(out/'runner.json').open('wb') as result:
  app=subprocess.Popen([str(stage/'bin/kde-index'),'--models',str(models),'--data-dir',str(data),'--socket',front,'--query','CARD-39312','--capture',str(out/'app.png'),'--capture-delay','12000'],stdout=app_log,stderr=app_log,env=env)
  time.sleep(.15)
  runner=subprocess.Popen([str(check),str(stage/'lib/qt6/plugins'),str(out/'runner.ini'),front,'find CARD-39312',str(corpus/'payment.txt')],stdout=result,stderr=runner_log,env=env)
  report['runner_exit']=runner.wait(timeout=25);report['app_exit']=app.wait(timeout=30)
 known.update(descendants(proc.pid));report['service_survives_unowned_app_capture']=proc.poll() is None
 intervals=[];starts={}
 for event in report['events']:
  raw=event['raw'];key=(event['client'],raw.get('id'))
  if event['direction']=='request' and raw.get('op')=='search':starts[key]=event
  elif event['direction']=='reply' and key in starts:
   start=starts.pop(key);intervals.append({'client':key[0],'id':key[1],'begin_ns':start['monotonic_ns'],'end_ns':event['monotonic_ns'],'text':start['raw'].get('text'),'ok':raw.get('ok'),'results':raw.get('results')})
 report['search_intervals']=intervals
 ui=[i for i in intervals if str(i['id']).startswith('ui-')];rr=[i for i in intervals if not str(i['id']).startswith('ui-')]
 report['overlap_observed']=any(max(u['begin_ns'],r['begin_ns'])<min(u['end_ns'],r['end_ns']) for u in ui for r in rr)
 report['pass']=report['runner_exit']==0 and report['app_exit']==0 and report['overlap_observed'] and report['service_survives_unowned_app_capture'] and all(i['ok'] and i['results'][0]['path']==str(corpus/'payment.txt') for i in ui+rr)
 req('shutdown');proc.wait(timeout=15)
except Exception as e:report['error']=f'{type(e).__name__}: {e}';report['pass']=False
finally:
 if app and app.poll() is None:app.terminate();app.wait(timeout=5)
 if runner and runner.poll() is None:runner.terminate();runner.wait(timeout=5)
 stop.set()
 if listener:listener.close()
 for peer in peers:
  try:peer.shutdown(socket.SHUT_RDWR)
  except OSError:pass
 for t in threads:t.join(timeout=3)
 if 'accept_thread' in globals():accept_thread.join(timeout=3)
 if client:client.close()
 if proc.poll() is None:proc.terminate();proc.wait(timeout=15)
 log.close()
 if Path(front).exists():Path(front).unlink()
 deadline=time.monotonic()+10
 while any(Path(f'/proc/{pid}').exists() for pid in known) and time.monotonic()<deadline:time.sleep(.1)
 report['closure']={'service_exit':proc.returncode,'known_pids':sorted(known),'all_known_pids_absent':all(not Path(f'/proc/{pid}').exists() for pid in known),'sockets_absent':not Path(back).exists() and not Path(front).exists(),'relay_threads_closed':all(not t.is_alive() for t in threads) and ('accept_thread' not in globals() or not accept_thread.is_alive())}
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ('pass','error','overlap_observed','closure')}))
raise SystemExit(0 if report.get('pass') and report['closure']['all_known_pids_absent'] and report['closure']['sockets_absent'] else 1)
