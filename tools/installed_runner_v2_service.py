#!/usr/bin/env python3
"""Bounded installed-discovery proof on an owned synthetic root and pinned CPU service."""
import hashlib,json,os,shutil,socket,subprocess,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def descendants(pid):
 found=set();pending=[pid]
 while pending:
  current=pending.pop()
  if current in found:continue
  found.add(current)
  for f in Path(f'/proc/{current}/task').glob('*/children'):
   try:pending.extend(map(int,f.read_text().split()))
   except FileNotFoundError:pass
 return found
out=ROOT/'local/runner-discovery-v1/service-run-v2'
if out.exists():raise SystemExit('fresh output required')
out.mkdir(); corpus=out/'corpus';corpus.mkdir()
shutil.copy2(ROOT/'evidence/ui-v2/inputs/payment.txt',corpus/'payment.txt')
for n in ('config','cache','data-home'):(out/n).mkdir()
env={**os.environ,'XDG_CONFIG_HOME':str(out/'config'),'XDG_CACHE_HOME':str(out/'cache'),'XDG_DATA_HOME':str(out/'data-home'),'XDG_DATA_DIRS':str(out/'data-home')}
stage=ROOT/'local/installed-host-v2/stage/usr';service=stage/'bin/kde-index-service';probe=stage/'bin/embedding-probe'
assert sha(service)=='3176fa48374a69e35b2b6b064e68dc86baf852dfd73d49c66716c569fe575845'
assert sha(probe)=='3dd4ddd9a133e9861ec834a2434121f99872b2ac94b1b211690a620357408414'
back=f'/tmp/kde-index-discovery-{os.getpid()}'
report={'scope':'installed native KPluginMetaData discovery and implicit RunnerManager against pinned CPU service; selected synthetic text only','identities':{p.name:sha(p) for p in (service,probe)},'requests':[],'native':[]}
log=(out/'service.log').open('wb');proc=None;conn=None;sequence=0;buffer=b'';known=set();stop=threading.Event()
def req(op,**fields):
 global sequence,buffer
 sequence+=1;payload={'id':f'proof-{sequence}','op':op,**fields};conn.sendall(json.dumps(payload).encode()+b'\n')
 while b'\n' not in buffer:
  chunk=conn.recv(65536)
  if not chunk:raise RuntimeError('service EOF')
  buffer+=chunk
 line,buffer=buffer.split(b'\n',1);reply=json.loads(line);report['requests'].append({'request':payload,'reply':reply})
 if reply.get('id')!=payload['id'] or reply.get('ok') is not True:raise RuntimeError(str(reply))
 return reply
try:
 proc=subprocess.Popen([str(service),'--data-dir',str(out/'index'),'--socket',back,'--probe',str(probe),'--probe-sha256',sha(probe),'--text-model',str(ROOT/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(ROOT/'models/mmproj-embeddinggemma-2-Q8_0.gguf')],env=env,stdout=log,stderr=log);known.add(proc.pid)
 def sample():
  while not stop.wait(.02):known.update(descendants(proc.pid))
 sampler=threading.Thread(target=sample);sampler.start()
 deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  if proc.poll() is not None:raise RuntimeError('service startup exit')
  conn=socket.socket(socket.AF_UNIX)
  try:conn.connect(back);conn.settimeout(120);break
  except OSError:conn.close();conn=None;time.sleep(.05)
 if conn is None:raise TimeoutError('socket')
 deadline=time.monotonic()+50
 while not req('status')['status']['ready']:
  if time.monotonic()>deadline:raise TimeoutError('ready')
  time.sleep(.1)
 req('add_root',path=str(corpus));deadline=time.monotonic()+120
 while True:
  status=req('status')['status']
  if status['error']:raise RuntimeError(status['error'])
  if status['indexed']==1 and not status['busy']:break
  if time.monotonic()>deadline:raise TimeoutError('index')
  time.sleep(.1)
 report['indexed_status']=status
 report['direct_exact_reply']=req('search',text='CARD-39312',limit=12)
 # Confirm producer unload before cold native query; no repeated query or inference.
 deadline=time.monotonic()+20
 while descendants(proc.pid)!={proc.pid}:
  if time.monotonic()>deadline:raise TimeoutError('producer unload')
  time.sleep(.1)
 report['before_cold_native']={'monotonic_ns':time.monotonic_ns(),'service_pid':proc.pid,'descendants':sorted(descendants(proc.pid))}
 controls=[('v3-derived-id',ROOT/'build-runner-discovery-v1-helper/installed-runner-v2',stage/'lib/qt6/plugins','libkde-index-runner'),('v4-stable-id',ROOT/'build-runner-discovery-v1-helper/installed-runner-v2',ROOT/'local/runner-discovery-v1/stage/usr/lib/qt6/plugins',None),('v4-original-helper',ROOT/'build-installed-runner/installed-runner',ROOT/'local/runner-discovery-v1/stage/usr/lib/qt6/plugins',None)]
 for name,helper,pluginroot,expected in controls:
  command=[str(helper),str(pluginroot),str(out/(name+'.ini')),back,'find CARD-39312',str(corpus/'payment.txt')]
  if expected:command.append(expected)
  begin=time.monotonic_ns();native=subprocess.Popen(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE);known.add(native.pid)
  try:stdout,stderr=native.communicate(timeout=20)
  except subprocess.TimeoutExpired:
   native.terminate();stdout,stderr=native.communicate(timeout=5);raise TimeoutError(name+' native query')
  p=subprocess.CompletedProcess(command,native.returncode,stdout,stderr);end=time.monotonic_ns()
  (out/(name+'.json')).write_bytes(p.stdout);(out/(name+'.stderr')).write_bytes(p.stderr)
  report['native'].append({'name':name,'pid':native.pid,'reaped':native.poll() is not None,'helper_sha256':sha(helper),'exit':p.returncode,'begin_ns':begin,'end_ns':end,'raw':json.loads(p.stdout)})
  if p.returncode!=0:raise RuntimeError(name+' failed')
 report['service_alive_after_clients']=proc.poll() is None
 req('shutdown');proc.wait(timeout=15);report['pass']=True
except Exception as e:report['error']=f'{type(e).__name__}: {e}';report['pass']=False
finally:
 if conn:conn.close()
 if proc and proc.poll() is None:proc.terminate();proc.wait(timeout=15)
 stop.set()
 if 'sampler' in globals():sampler.join(timeout=3)
 log.close();deadline=time.monotonic()+10
 while any(Path(f'/proc/{pid}').exists() for pid in known) and time.monotonic()<deadline:time.sleep(.1)
 report['closure']={'service_exit':proc.returncode if proc else None,'known_pids':sorted(known),'all_known_pids_absent':all(not Path(f'/proc/{pid}').exists() for pid in known),'socket_absent':not Path(back).exists(),'sampler_closed':'sampler' not in globals() or not sampler.is_alive()}
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ('pass','error','closure')}));raise SystemExit(0 if report.get('pass') and all(report['closure'][k] for k in ('all_known_pids_absent','socket_absent','sampler_closed')) else 1)
