#!/usr/bin/env python3
"""One real app/portal/read-only Konsole receiver test in an owned recommended-handler profile."""
import argparse,hashlib,json,os,select,signal,socket,struct,subprocess,threading,time,shutil
from pathlib import Path
P=Path(__file__).resolve().parents[1];sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--stage',type=Path);p.add_argument('--output',type=Path);p.add_argument('--session',action='store_true');a=p.parse_args()
if not a.session:
 out=a.output.resolve();stage=a.stage.resolve();assert not out.exists();out.mkdir(parents=True)
 for n in ['home','config','cache','data','runtime','app-observer','receivers']:(out/n).mkdir()
 (out/'runtime').chmod(0o700);shutil.copy2(__file__,out/'run.py')
 for source,target in [(P/'build-desktop-open-v3/build/libdesktop-open-v3-observer.so','receiver-observer.so'),(P/'build-desktop-open-v3/build/desktop-open-v3-gio','gio-inspect'),(P/'evidence/desktop-boundary-v2/candidate/bin/libdesktop-layout-observer.so','app-observer.so')]:shutil.copy2(source,out/target)
 cmd=['bwrap','--ro-bind','/','/','--tmpfs','/home','--tmpfs','/root','--tmpfs','/run','--tmpfs','/tmp','--unshare-pid','--unshare-net','--proc','/proc','--dev','/dev','--bind',str(out),'/tmp/work','--ro-bind',str(stage),'/tmp/stage','--ro-bind',str(P/'models'),'/tmp/models','--ro-bind',os.environ['XDG_RUNTIME_DIR']+'/'+os.environ['WAYLAND_DISPLAY'],'/tmp/work/runtime/wayland-0','--clearenv']
 env={'PATH':'/tmp/stage/bin:/usr/bin:/bin','HOME':'/tmp/work/home','XDG_CONFIG_HOME':'/tmp/work/config','XDG_CACHE_HOME':'/tmp/work/cache','XDG_DATA_HOME':'/tmp/work/data','XDG_RUNTIME_DIR':'/tmp/work/runtime','XDG_DATA_DIRS':'/tmp/stage/share:/usr/share','QT_PLUGIN_PATH':'/tmp/stage/lib/qt6/plugins:/usr/lib/qt6/plugins','WAYLAND_DISPLAY':'wayland-0','QT_QPA_PLATFORM':'wayland','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8','XDG_CURRENT_DESKTOP':'KDE','XDG_SESSION_TYPE':'wayland','DESKTOP_SESSION':'plasma','G_MESSAGES_DEBUG':'xdg-desktop-portal','LD_PRELOAD':'/tmp/work/receiver-observer.so','DESKTOP_OPEN_V3_OBSERVER_ROOT':'/tmp/work/receivers'}
 for k,v in env.items():cmd+=['--setenv',k,v]
 cmd+=['dbus-run-session','--','python','/tmp/work/run.py','--session'];start=time.monotonic();r=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
 try:stdout,_=r.communicate(timeout=120)
 except subprocess.TimeoutExpired:os.killpg(r.pid,signal.SIGKILL);stdout,_=r.communicate();(out/'session.log').write_bytes(stdout);(out/'outer.json').write_text(json.dumps({'namespace_exit':r.returncode,'hard_bound_seconds':120,'error':'outer hard bound; namespace killed','elapsed_seconds':time.monotonic()-start},indent=2)+'\n');raise
 (out/'session.log').write_bytes(stdout);outer={'namespace_exit':r.returncode,'elapsed_seconds':time.monotonic()-start,'isolation':'private PID/network/D-Bus/profile; ro root/public stage/models; masked personal homes/run/tmp; same Wayland socket'};(out/'outer.json').write_text(json.dumps(outer,indent=2)+'\n');print(json.dumps(outer));raise SystemExit(r.returncode)
O=Path('/tmp/work');report={'scope':'one controlled private single-recommended-handler real portal open plus independent source calibration','events':[]};owned=[];logs=[];threads=[];peers=[];stop=threading.Event();client=None;listener=None;service=None
corpus=O/'corpus';corpus.mkdir();source=corpus/'reader.txt';source.write_bytes(b'Amber maps ride quiet lanterns 59317428.\n');source.chmod(0o444)
calibration=O/'calibration';calibration.mkdir();contrast=calibration/'contrast.txt';contrast.write_bytes(b'Violet rails hold calm beacons 78162044.\n');contrast.chmod(0o444)
report['sources']={str(f):{'sha256':sha(f),'inode':f.stat().st_ino,'mode':oct(f.stat().st_mode&0o777),'bytes_utf8':f.read_text()} for f in [source,contrast]};report['binaries']={str(f):sha(f) for f in [Path('/tmp/stage/bin/kde-index'),Path('/tmp/stage/bin/kde-index-service'),Path('/tmp/stage/bin/embedding-probe'),Path('/usr/bin/konsole'),Path('/usr/bin/cat'),Path('/tmp/work/receiver-observer.so'),Path('/tmp/work/app-observer.so')]}
apps=O/'data/applications';apps.mkdir();handler=apps/'desktop-source-consumer.desktop';handler.write_text('[Desktop Entry]\nType=Application\nName=Owned source reader\nExec=/usr/bin/konsole --separate --nofork --hold -e /usr/bin/cat %f\nMimeType=text/plain;\nNoDisplay=false\nTerminal=false\n');(O/'config/mimeapps.list').write_text('[Default Applications]\ntext/plain=desktop-source-consumer.desktop;\n')

def wait(fn,bound=15):
 deadline=time.monotonic()+bound
 while time.monotonic()<deadline:
  r=fn()
  if r:return r
  time.sleep(.04)
 raise TimeoutError('declared bounded condition')
def spawn(args,name,env_changes=None):
 f=(O/(name+'.log')).open('wb');logs.append(f);r=subprocess.Popen(args,env={**os.environ,**(env_changes or {})},stdout=f,stderr=f);owned.append(r);return r

def receiver_command(pid,name,capture=False):
 d=O/'receivers'/str(pid);wait(lambda:(d/'identity.json').exists(),10);tmp=d/'command.tmp';tmp.write_text(json.dumps({'id':name,'op':'capture' if capture else 'snapshot'}));tmp.rename(d/'command.json');f=d/(name+'.json');wait(f.exists,5);r=json.loads(f.read_text());r['actual_process']={'pid':pid,'executable':os.readlink(f'/proc/{pid}/exe'),'argv':Path(f'/proc/{pid}/cmdline').read_bytes().replace(b'\0',b' ').decode(),'startticks':Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()[19],'loaded_konsole_maps':[s for s in Path(f'/proc/{pid}/maps').read_text().splitlines() if any(v in s for v in ['libkonsole','receiver-observer.so'])]};f.write_text(json.dumps(r,indent=2)+'\n');return r

def native_first_lines(r):
 lines=[]
 for t in r['terminals']:
  if t.get('text_interface_present') and t.get('accessible_valid'):
   s=t['actual_native_text'].replace('\r\n','\n').replace('\r','\n');nonempty=[line.rstrip(' ') for line in s.split('\n') if line.rstrip(' ')]
   if nonempty:lines.append(nonempty[0])
 return lines

def app_command(name,op='snapshot',**fields):
 d=O/'app-observer';tmp=d/'command.tmp';tmp.write_text(json.dumps({'id':name,'op':op,**fields}));tmp.rename(d/'command.json');f=d/(name+'.json');wait(f.exists,5);return json.loads(f.read_text())
seq=0;buffer=b''
def req(op,**kw):
 global seq,buffer
 seq+=1;ident='control-'+str(seq);client.sendall(json.dumps({'id':ident,'op':op,**kw}).encode()+b'\n')
 while b'\n' not in buffer:
  b=client.recv(65536)
  if not b:raise RuntimeError('service EOF')
  buffer+=b
 line,buffer=buffer.split(b'\n',1);r=json.loads(line);assert r['id']==ident and r['ok'],r;return r

def relay(peer,index):
 dest=socket.socket(socket.AF_UNIX);dest.connect('/tmp/work/service.sock');peers.extend([peer,dest]);pid,uid,gid=struct.unpack('3i',peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12));report.setdefault('ipc_peers',[]).append({'connection':index,'pid':pid,'uid':uid,'gid':gid,'executable':os.readlink(f'/proc/{pid}/exe'),'argv':Path(f'/proc/{pid}/cmdline').read_bytes().replace(b'\0',b' ').decode()});buffers={peer:b'',dest:b''}
 try:
  while not stop.is_set():
   for s in select.select([peer,dest],[],[],.1)[0]:
    b=s.recv(65536)
    if not b:return
    buffers[s]+=b
    while b'\n' in buffers[s]:
     line,buffers[s]=buffers[s].split(b'\n',1);report['events'].append({'connection':index,'direction':'request' if s is peer else 'reply','monotonic_ns':time.monotonic_ns(),'raw':json.loads(line)})
    (dest if s is peer else peer).sendall(b)
 except OSError:pass
 finally:peer.close();dest.close()
def accept():
 i=0
 while not stop.is_set():
  try:peer,_=listener.accept()
  except socket.timeout:continue
  except OSError:return
  t=threading.Thread(target=relay,args=(peer,i));threads.append(t);t.start();i+=1
try:
 cache=subprocess.run(['/usr/bin/update-desktop-database',str(apps)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=10);report['mime_cache']={'exit':cache.returncode,'stdout':cache.stdout.decode(),'stderr':cache.stderr.decode()};assert cache.returncode==0
 gi=subprocess.run(['/tmp/work/gio-inspect'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=10);report['gio_before_open']=json.loads(gi.stdout);(O/'gio-before-open.json').write_bytes(gi.stdout);g=report['gio_before_open'];assert gi.returncode==0 and g['default']=='desktop-source-consumer.desktop' and len(g['choices'])==1 and g['choices'][0]['id']=='desktop-source-consumer.desktop',g
 # Reuse checked immutable run1 source B/A calibration for the byte-identical receiver observer.
 expected_b=contrast.read_text().rstrip('\n');expected_a=source.read_text().rstrip('\n')
 report['calibration_reused']={'evidence':'desktop-open-v3 run1 receiver8 calibration-capture','observer_sha256':sha('/tmp/work/receiver-observer.so'),'scope':'actual Konsole source-B positive/target-A negative, same source/native observer bytes; no second calibration launch'}
 before=set(p.name for p in (O/'receivers').iterdir());report['receiver_directories_before_open']=sorted(before)
 service=spawn(['/tmp/stage/bin/kde-index-service','--socket','/tmp/work/service.sock','--data-dir','/tmp/work/index','--probe','/tmp/stage/bin/embedding-probe','--probe-sha256',sha('/tmp/stage/bin/embedding-probe'),'--text-model','/tmp/models/embeddinggemma-2-Q8_0.gguf','--image-model','/tmp/models/mmproj-embeddinggemma-2-Q8_0.gguf'],'service');wait(lambda:Path('/tmp/work/service.sock').exists(),25);client=socket.socket(socket.AF_UNIX);client.settimeout(40);client.connect('/tmp/work/service.sock');wait(lambda:req('status')['status']['ready'],30);req('add_root',path=str(corpus));report['indexed_status']=wait(lambda:(s if (s:=req('status')['status'])['indexed']==1 and not s['busy'] else None),30)
 listener=socket.socket(socket.AF_UNIX);listener.bind('/tmp/work/relay.sock');listener.listen();listener.settimeout(.1);at=threading.Thread(target=accept);at.start()
 monitor=spawn(['/usr/bin/dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'","interface='org.freedesktop.impl.portal.AppChooser'","interface='org.freedesktop.portal.Request'","type='method_return'","type='error'"],'portal-monitor');wait(lambda:'NameAcquired' in (O/'portal-monitor.log').read_text(errors='replace'),5)
 app=spawn(['/tmp/stage/bin/kde-index','--socket','/tmp/work/relay.sock','--models','/tmp/models','--data-dir','/tmp/work/index','--query','reader.txt'],'app',{'LD_PRELOAD':'/tmp/work/app-observer.so','DESKTOP_LAYOUT_OUT':'/tmp/work/app-observer'})
 def source_ready():
  r=app_command('source-ready-'+str(time.monotonic_ns()));nodes=r['snapshot']['nodes']
  labels=[n for n in nodes if n.get('class','').startswith('Label_') and n.get('text')==source.name and n.get('geometry_intersects_viewport')]
  rows=[row for n in nodes for row in n.get('actual_backend_state',{}).get('results',[]) if row.get('path')==str(source) and row.get('title')==source.name]
  completed=[e for e in report['events'] if e['direction']=='reply' and e['raw'].get('ok') and e['raw'].get('protocol')==1 and any(row.get('path')==str(source) and row.get('title')==source.name for row in e['raw'].get('results',[]))]
  buttons=[n for n in nodes if n.get('text')=='Open file' and n.get('class','').startswith('ToolButton') and n.get('geometry_intersects_viewport')]
  return (r,buttons) if labels and rows and completed and len(buttons)==1 else None
 ready,buttons=wait(source_ready,15);report['app_source_phase_receipt']=ready['command']['id']
 report['open_action_monotonic_ns']=time.monotonic_ns();report['open_signal_invoked']=app_command('actual-open-signal','invoke-open-signal',node=buttons[0]['node'])['signal_invoked'];assert report['open_signal_invoked']
 def loaded_receiver():
  for d in (O/'receivers').iterdir():
   if d.name not in before and (d/'identity.json').exists():
    pid=int(d.name);r=receiver_command(pid,'loaded-'+str(time.monotonic_ns()));lines=native_first_lines(r)
    if expected_a in lines:return (pid,r,lines)
  return None
 pid,loaded,lines=wait(loaded_receiver,20);report['receiving_app']={'pid':pid,'receipt':loaded['command']['id'],'actual_first_lines':lines,'target_source_line_matches':expected_a in lines,'contrast_source_line_absent':expected_b not in lines,'first_line_payload_sha256':hashlib.sha256((lines[0]+'\n').encode('ascii')).hexdigest(),'process':loaded['actual_process']};assert report['receiving_app']['first_line_payload_sha256']==sha(source)
 final=receiver_command(pid,'actual-receiver-capture',True);report['receiving_capture']='receivers/'+str(pid)+'/actual-receiver-capture.json'
 report['source_unchanged']=sha(source)==report['sources'][str(source)]['sha256'];report['open_api_events']=[json.loads(s) for s in (O/'app-observer/open-events.jsonl').read_text().splitlines()];report['portal_trace']=(O/'portal-monitor.log').read_text(errors='replace');report['app_after_open']=app.poll();report['pass']=report['source_unchanged'] and report['receiving_app']['target_source_line_matches'] and report['receiving_app']['contrast_source_line_absent']
 req('shutdown');service.wait(timeout=10)
except Exception as e:report['error']=repr(e);report['pass']=False
finally:
 stop.set()
 if listener:listener.close()
 for s in peers:
  try:s.shutdown(socket.SHUT_RDWR)
  except OSError:pass
 for t in threads:t.join(2)
 if 'at' in globals():at.join(2)
 if client:client.close()
 for r in reversed(owned):
  if r.poll() is None:r.terminate()
  try:r.wait(timeout=5)
  except subprocess.TimeoutExpired:r.kill();r.wait()
 extras=[]
 for path in Path('/proc').iterdir():
  if path.name.isdigit() and int(path.name) not in (1,os.getpid(),os.getppid()):
   try:pid=int(path.name);extras.append({'pid':pid,'argv':(path/'cmdline').read_bytes().replace(b'\0',b' ').decode()});os.kill(pid,15)
   except OSError:pass
 for f in logs:f.close()
 for name in ['service.sock','relay.sock']:
  f=O/name
  if f.exists():f.unlink()
 report['closure']={'owned_exits':[{'pid':r.pid,'exit':r.returncode} for r in owned],'relay_threads_closed':all(not t.is_alive() for t in threads),'accept_thread_closed':'at' not in globals() or not at.is_alive(),'exclusive_namespace_extras_terminated':extras,'owned_socket_files_absent':not any((O/n).exists() for n in ['service.sock','relay.sock'])};(O/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ['pass','error','calibration','receiving_app','closure']}));raise SystemExit(0 if report.get('pass') else 1)
