#!/usr/bin/env python3
"""One source-faithful stock calculator/plugin pair plus one passive portal open trace."""
import argparse,hashlib,json,os,select,signal,socket,struct,subprocess,threading,time,shutil
from pathlib import Path
P=Path(__file__).resolve().parents[1];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--stage',type=Path);p.add_argument('--output',type=Path);p.add_argument('--session',action='store_true');a=p.parse_args()
if not a.session:
 out=a.output.resolve();stage=a.stage.resolve();assert not out.exists();out.mkdir(parents=True)
 for n in ['home','config','cache','data','runtime','runner-observer','app-observer']:(out/n).mkdir()
 (out/'runtime').chmod(0o700);shutil.copy2(__file__,out/'pair.py');shutil.copy2(P/'build-desktop-layout-v2/build/libdesktop-layout-observer.so',out/'libdesktop-layout-observer.so')
 cmd=['bwrap','--ro-bind','/','/','--tmpfs','/home','--tmpfs','/root','--tmpfs','/run','--tmpfs','/tmp','--unshare-pid','--unshare-net','--proc','/proc','--dev','/dev','--bind',str(out),'/tmp/work','--ro-bind',str(stage),'/tmp/stage','--ro-bind',str(P/'models'),'/tmp/models','--ro-bind',os.environ['XDG_RUNTIME_DIR']+'/'+os.environ['WAYLAND_DISPLAY'],'/tmp/work/runtime/wayland-0','--clearenv']
 env={'PATH':'/tmp/stage/bin:/usr/bin:/bin','HOME':'/tmp/work/home','XDG_CONFIG_HOME':'/tmp/work/config','XDG_CACHE_HOME':'/tmp/work/cache','XDG_DATA_HOME':'/tmp/work/data','XDG_RUNTIME_DIR':'/tmp/work/runtime','XDG_DATA_DIRS':'/tmp/stage/share:/usr/share','QT_PLUGIN_PATH':'/tmp/stage/lib/qt6/plugins:/usr/lib/qt6/plugins','WAYLAND_DISPLAY':'wayland-0','QT_QPA_PLATFORM':'wayland','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8','XDG_CURRENT_DESKTOP':'KDE','XDG_SESSION_TYPE':'wayland','DESKTOP_SESSION':'plasma','KDE_INDEX_SOCKET':'/tmp/work/relay.sock','G_MESSAGES_DEBUG':'all','QT_LOGGING_RULES':'xdp-kde-app-chooser.debug=true'}
 for k,v in env.items():cmd+=['--setenv',k,v]
 cmd+=['dbus-run-session','--','python','/tmp/work/pair.py','--session'];started=time.monotonic();r=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
 try:stdout,_=r.communicate(timeout=180)
 except subprocess.TimeoutExpired:os.killpg(r.pid,signal.SIGKILL);stdout,_=r.communicate();(out/'session.log').write_bytes(stdout);raise
 (out/'session.log').write_bytes(stdout);outer={'namespace_exit':r.returncode,'elapsed_seconds':time.monotonic()-started,'isolation':'ro-host; private PID/network/D-Bus; personal homes/run/tmp masked; owned XDG; same Wayland socket'};(out/'outer.json').write_text(json.dumps(outer,indent=2)+'\n');print(json.dumps(outer));raise SystemExit(r.returncode)
O=Path('/tmp/work');events=[];peers=[];threads=[];owned=[];logs=[];stop=threading.Event();report={'scope':'one ordinary stock calculator/plugin matched pair and passive real portal request, diagnostic only','events':events,'environment':{k:os.environ[k] for k in ['QT_QUICK_BACKEND','QT_QPA_PLATFORM','XDG_CURRENT_DESKTOP','XDG_SESSION_TYPE','XDG_RUNTIME_DIR','XDG_CONFIG_HOME']}}
corpus=O/'corpus';corpus.mkdir();source=corpus/'ordinary-layout-66442.txt';source.write_text('NATIVE-LAYOUT-66442 plain owned text for stock geometry and source opening.\n')
(O/'data/applications').mkdir();(O/'data/applications/desktop-boundary-text-opener.desktop').write_text('[Desktop Entry]\nType=Application\nName=Owned desktop boundary opener\nExec=/usr/bin/dolphin --select %u\nMimeType=text/plain;\nNoDisplay=true\n');(O/'config/mimeapps.list').write_text('[Default Applications]\ntext/plain=desktop-boundary-text-opener.desktop;\n')
report['source']={'path':str(source),'sha256':sha(source),'inode':source.stat().st_ino,'contents':source.read_text()};report['binaries']={str(f):sha(f) for f in [Path('/tmp/stage/bin/kde-index'),Path('/tmp/stage/bin/kde-index-service'),Path('/tmp/stage/bin/embedding-probe'),Path('/tmp/stage/lib/qt6/plugins/kf6/krunner/kde-index-runner.so'),Path('/usr/bin/krunner')]}
def spawn(args,role=None):
 env=dict(os.environ)
 if role:env.update(LD_PRELOAD='/tmp/work/libdesktop-layout-observer.so',DESKTOP_LAYOUT_OUT='/tmp/work/'+role+'-observer')
 f=(O/((role or Path(args[0]).name)+'.log')).open('wb');logs.append(f);r=subprocess.Popen(args,env=env,stdout=f,stderr=f);owned.append(r);return r

def wait(fn,bound=15):
 end=time.monotonic()+bound
 while time.monotonic()<end:
  v=fn()
  if v:return v
  time.sleep(.04)
 raise TimeoutError('declared bounded condition')
seq=0;buf=b'';client=None;listener=None;service=None
def req(op,**kw):
 global seq,buf
 seq+=1;ident='control-'+str(seq);client.sendall(json.dumps({'id':ident,'op':op,**kw}).encode()+b'\n')
 while b'\n' not in buf:
  b=client.recv(65536)
  if not b:raise RuntimeError('service EOF')
  buf+=b
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==ident and r['ok'],r;return r

def observer(role,name,op='snapshot',**fields):
 d=O/(role+'-observer');f=d/(name+'.json');tmp=d/'command.tmp';tmp.write_text(json.dumps({'id':name,'op':op,**fields}));tmp.rename(d/'command.json');wait(f.exists,15);r=json.loads(f.read_text());snapshot=r.get('snapshot',r.get('pre_readback',{}));pid=snapshot.get('pid');r['actual_loaded_maps']=[x for x in Path(f'/proc/{pid}/maps').read_text().splitlines() if any(v in x for v in ['kde-index-runner.so','calculator.so','PlatformTheme'])];r['host_monotonic_ns']=time.monotonic_ns();f.write_text(json.dumps(r,indent=2)+'\n');return r

def relay(peer,index):
 dest=socket.socket(socket.AF_UNIX);dest.connect('/tmp/work/service.sock');peers.extend([peer,dest]);pid,uid,gid=struct.unpack('3i',peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12));report.setdefault('ipc_peers',[]).append({'connection':index,'pid':pid,'uid':uid,'gid':gid,'executable':os.readlink(f'/proc/{pid}/exe'),'cmdline':Path(f'/proc/{pid}/cmdline').read_bytes().replace(b'\0',b' ').decode()});buffers={peer:b'',dest:b''}
 try:
  while not stop.is_set():
   for s in select.select([peer,dest],[],[],.1)[0]:
    b=s.recv(65536)
    if not b:return
    buffers[s]+=b
    while b'\n' in buffers[s]:
     line,buffers[s]=buffers[s].split(b'\n',1);events.append({'connection':index,'direction':'request' if s is peer else 'reply','monotonic_ns':time.monotonic_ns(),'raw':json.loads(line)})
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

def query(runner,text):
 r=subprocess.run(['/usr/bin/qdbus6','org.kde.krunner','/App','org.kde.krunner.App.querySingleRunner',runner,text],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5);report.setdefault('query_calls',[]).append({'runner':runner,'query':text,'exit':r.returncode,'stdout':r.stdout.decode(),'stderr':r.stderr.decode(),'monotonic_ns':time.monotonic_ns()});return r.returncode==0
try:
 service=spawn(['/tmp/stage/bin/kde-index-service','--socket','/tmp/work/service.sock','--data-dir','/tmp/work/index','--probe','/tmp/stage/bin/embedding-probe','--probe-sha256',sha('/tmp/stage/bin/embedding-probe'),'--text-model','/tmp/models/embeddinggemma-2-Q8_0.gguf','--image-model','/tmp/models/mmproj-embeddinggemma-2-Q8_0.gguf'])
 wait(lambda:Path('/tmp/work/service.sock').exists(),30);client=socket.socket(socket.AF_UNIX);client.settimeout(50);client.connect('/tmp/work/service.sock');wait(lambda:req('status')['status']['ready'],40);req('add_root',path=str(corpus));report['indexed_status']=wait(lambda:(r if (r:=req('status')['status'])['indexed']==1 and not r['busy'] else None),40)
 listener=socket.socket(socket.AF_UNIX);listener.bind('/tmp/work/relay.sock');listener.listen();listener.settimeout(.1);at=threading.Thread(target=accept);at.start()
 inventory=subprocess.run(['/usr/bin/krunner','--list'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=10);(O/'stock-plugin-list.txt').write_bytes(inventory.stdout);report['plugin_discovery_exit']=inventory.returncode;assert inventory.returncode==0 and ': calculator' in inventory.stdout.decode() and ': kde-index-runner' in inventory.stdout.decode(),inventory.stdout
 runner=spawn(['/usr/bin/krunner','--daemon'],'runner');wait(lambda:query('calculator','=1+2'),10);report['builtin_completed']=observer('runner','builtin-completed','capture-complete',query='=1+2',expected_label='3',bound_ms=10000)['query_completed_phase']
 assert query('kde-index-runner','find NATIVE-LAYOUT-66442');report['plugin_completed']=observer('runner','plugin-completed','capture-complete',query='find NATIVE-LAYOUT-66442',expected_label=source.name,bound_ms=10000)['query_completed_phase']
 # No geometry intervention or repeated native pair. Open observation is passive through the real handler.
 monitor=spawn(['/usr/bin/dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'","interface='org.freedesktop.impl.portal.AppChooser'","interface='org.freedesktop.portal.Request'","interface='org.freedesktop.impl.portal.Request'","type='method_return'","type='error'"])
 app=spawn(['/tmp/stage/bin/kde-index','--socket','/tmp/work/relay.sock','--models','/tmp/models','--data-dir','/tmp/work/index','--query','NATIVE-LAYOUT-66442'],'app')
 def app_source_ready():
  r=observer('app','app-source-'+str(time.monotonic_ns()));return r if any(n.get('text')==source.name and n.get('geometry_intersects_viewport') for n in r['snapshot']['nodes']) else None
 app_snapshot=wait(app_source_ready,12);report['app_source_phase_receipt']=app_snapshot['command']['id']
 opens=[n for n in app_snapshot['snapshot']['nodes'] if n.get('text')=='Open file' and n.get('geometry_intersects_viewport') and n.get('class','').startswith('ToolButton')];assert len(opens)==1,opens
 report['open_signal_invoked']=observer('app','actual-open-signal','invoke-open-signal',node=opens[0]['node'])['signal_invoked']
 # A protocol event is the endpoint, not a sleep. Never accept or fabricate the chooser result.
 def portal_endpoint():
  text=(O/'dbus-monitor.log').read_text(errors='replace')
  return 'member=ChooseApplication' in text or ('interface=org.freedesktop.portal.Request' in text and 'member=Response' in text)
 try:wait(portal_endpoint,12);report['portal_endpoint']='actual backend chooser or request Response observed'
 except TimeoutError:report['portal_endpoint']='No backend chooser/Response within declared 12s; bounded unknown'
 report['portal_final_trace']=(O/'dbus-monitor.log').read_text(errors='replace');report['open_api_events']=[json.loads(s) for s in (O/'app-observer/open-events.jsonl').read_text().splitlines()] if (O/'app-observer/open-events.jsonl').exists() else []
 req('shutdown');service.wait(timeout=15);report['bounded_pair_completed']=True
except Exception as e:report['error']=repr(e)
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
   try:pid=int(path.name);extras.append({'pid':pid,'cmdline':(path/'cmdline').read_bytes().replace(b'\0',b' ').decode()});os.kill(pid,15)
   except OSError:pass
 for f in logs:f.close()
 for name in ['service.sock','relay.sock']:
  f=O/name
  if f.exists():f.unlink()
 report['closure']={'owned_exits':[{'pid':r.pid,'exit':r.returncode} for r in owned],'relay_threads_closed':all(not t.is_alive() for t in threads),'accept_thread_closed':'at' not in globals() or not at.is_alive(),'exclusive_namespace_extra_processes_terminated':extras,'owned_socket_files_absent':not any((O/n).exists() for n in ['service.sock','relay.sock'])};(O/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ['builtin_completed','plugin_completed','portal_endpoint','bounded_pair_completed','error','closure']}));raise SystemExit(0 if report.get('bounded_pair_completed') else 1)
