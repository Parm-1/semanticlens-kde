#!/usr/bin/env python3
"""Private stock KDE consumer probe; synthetic sources, source-derived fields only."""
import argparse, hashlib, http.server, signal, zlib, json, os, select, shutil, socket, struct, subprocess, threading, time, urllib.request
from pathlib import Path
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
P=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--stage',type=Path);p.add_argument('--output',type=Path);p.add_argument('--session',action='store_true');a=p.parse_args()
if not a.session:
 out=a.output.resolve();stage=a.stage.resolve();assert not out.exists();out.mkdir(parents=True)
 for n in ['home','config','cache','data','runtime','runner-observer','app-observer','dolphin-observer']: (out/n).mkdir()
 (out/'runtime').chmod(0o700)
 shutil.copy2(__file__,out/'probe.py');shutil.copy2(P/'build-desktop-integration-v1/build/libdesktop-observer.so',out/'libdesktop-observer.so')
 cmd=['bwrap','--ro-bind','/','/','--tmpfs','/home','--tmpfs','/root','--tmpfs','/run','--tmpfs','/tmp','--unshare-pid','--unshare-net','--proc','/proc','--dev','/dev','--bind',str(out),'/tmp/work','--ro-bind',str(stage),'/tmp/stage','--ro-bind',str(P/'models'),'/tmp/models','--ro-bind',os.environ['XDG_RUNTIME_DIR']+'/'+os.environ['WAYLAND_DISPLAY'],'/tmp/work/runtime/wayland-0','--clearenv']
 for k,v in {'PATH':'/tmp/work/bin:/tmp/stage/bin:/usr/bin:/bin','HOME':'/tmp/work/home','XDG_CONFIG_HOME':'/tmp/work/config','XDG_CACHE_HOME':'/tmp/work/cache','XDG_DATA_HOME':'/tmp/work/data','XDG_RUNTIME_DIR':'/tmp/work/runtime','XDG_DATA_DIRS':'/tmp/stage/share:/usr/share','QT_PLUGIN_PATH':'/tmp/stage/lib/qt6/plugins:/usr/lib/qt6/plugins','WAYLAND_DISPLAY':'wayland-0','QT_QPA_PLATFORM':'wayland','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8','XDG_CURRENT_DESKTOP':'KDE','XDG_SESSION_TYPE':'wayland','DESKTOP_SESSION':'plasma','KDE_INDEX_SOCKET':'/tmp/work/service-relay.sock'}.items():cmd+=['--setenv',k,v]
 cmd+=['dbus-run-session','--','python','/tmp/work/probe.py','--session']
 started=time.monotonic();proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
 try:stdout,_=proc.communicate(timeout=200)
 except subprocess.TimeoutExpired:
  os.killpg(proc.pid,signal.SIGKILL);stdout,_=proc.communicate();(out/'session.log').write_bytes(stdout);raise
 r=proc;(out/'session.log').write_bytes(stdout)
 print(json.dumps({'exit':r.returncode,'elapsed':time.monotonic()-started,'output':str(out)}));raise SystemExit(r.returncode)
O=Path('/tmp/work');(O/'bin').mkdir();
(O/'bin/dolphin').write_text('#!/bin/sh\nexport DESKTOP_OBSERVER_ROLE=dolphin DESKTOP_OBSERVER_OUT=/tmp/work/dolphin-observer LD_PRELOAD=/tmp/work/libdesktop-observer.so\nexec /usr/bin/dolphin "$@"\n');(O/'bin/dolphin').chmod(0o755)
# Owned MIME association makes QDesktopServices dispatch observable in real Dolphin.
(O/'data/applications').mkdir();(O/'data/applications/desktop-proof-open.desktop').write_text('[Desktop Entry]\nType=Application\nName=Owned desktop proof opener\nExec=/tmp/work/bin/dolphin --select %u\nMimeType=text/plain;\nNoDisplay=true\n');(O/'config/mimeapps.list').write_text('[Default Applications]\ntext/plain=desktop-proof-open.desktop;\n')
events=[];gets=[];owned=[];peers=[];stop=threading.Event();report={'scope':'stock /usr/bin/krunner and staged GUI, private session, owned synthetic corpus, diagnostic only','events':events,'http_gets':gets};logs=[]
def spawn(args,role=None):
 env=dict(os.environ)
 if role:env.update(LD_PRELOAD='/tmp/work/libdesktop-observer.so',DESKTOP_OBSERVER_ROLE=role,DESKTOP_OBSERVER_OUT='/tmp/work/'+role+'-observer')
 f=(O/((role or Path(args[0]).name)+'.log')).open('wb');logs.append(f);r=subprocess.Popen(args,env=env,stdout=f,stderr=f);owned.append(r);return r
def chunk(t,b):return struct.pack('!I',len(b))+t+b+struct.pack('!I',zlib.crc32(t+b))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',1,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0\xff'))+chunk(b'IEND',b'')
(O/'canary.png').write_bytes(png)
class HTTP(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  gets.append({'path':self.path,'monotonic_ns':time.monotonic_ns()});self.send_response(200);self.send_header('Content-Type','image/png');self.end_headers();self.wfile.write(png)
 def log_message(self,*a):pass
http=http.server.ThreadingHTTPServer(('127.0.0.1',0),HTTP);ht=threading.Thread(target=http.serve_forever,daemon=True);ht.start();port=http.server_port
corpus=O/'corpus';corpus.mkdir();title='<b>TITLE-77331.txt';source=corpus/title
source.write_text(f'DESKTOP-77331 <img src="http://127.0.0.1:{port}/passage" /> ' + 'Synthetic owned passage. '*20)
(corpus/'<b>ERROR-77331.pdf').write_bytes(b'invalid owned PDF')
report['sources']={str(f):sha(f) for f in corpus.iterdir()};report['stage_binary_sha256']={str(f):sha(f) for f in [Path('/tmp/stage/bin/kde-index-service'),Path('/tmp/stage/bin/embedding-probe'),Path('/tmp/stage/bin/kde-index'),Path('/tmp/stage/lib/qt6/plugins/kf6/krunner/kde-index-runner.so'),Path('/usr/bin/krunner'),Path('/usr/bin/dolphin')]}
service=spawn(['/tmp/stage/bin/kde-index-service','--socket','/tmp/work/service.sock','--data-dir','/tmp/work/index','--probe','/tmp/stage/bin/embedding-probe','--probe-sha256',sha('/tmp/stage/bin/embedding-probe'),'--text-model','/tmp/models/embeddinggemma-2-Q8_0.gguf','--image-model','/tmp/models/mmproj-embeddinggemma-2-Q8_0.gguf'])
client=None;listener=None;threads=[];seq=0;buf=b''
def wait(p,bound=20):
 end=time.monotonic()+bound
 while time.monotonic()<end:
  v=p()
  if v:return v
  time.sleep(.05)
 raise TimeoutError('bounded condition')
def req(op,**kw):
 global seq,buf
 seq+=1;id='control-'+str(seq);client.sendall(json.dumps({'id':id,'op':op,**kw}).encode()+b'\n')
 while b'\n' not in buf:buf+=client.recv(65536)
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==id and r['ok'],r;return r

def observe(role,id,op='snapshot',**kw):
 d=O/(role+'-observer');temp=d/'command.tmp';temp.write_text(json.dumps({'id':id,'op':op,**kw}));temp.rename(d/'command.json');f=d/(id+'.json');wait(f.exists,8);r=json.loads(f.read_text());pid=r['snapshot']['pid'];r['snapshot']['loaded_relevant_maps']=[line for line in Path(f'/proc/{pid}/maps').read_text().splitlines() if any(x in line for x in ['kde-index-runner.so','PlatformTheme','qxdgdesktopportal','dolphinprivate'])];r['snapshot']['observer_monotonic_ns']=time.monotonic_ns();f.write_text(json.dumps(r,indent=2)+'\n');return r
def find_nodes(s,predicate):return [n for n in s['snapshot']['nodes'] if predicate(n)]
def relay(peer,index):
 dest=socket.socket(socket.AF_UNIX);dest.connect('/tmp/work/service.sock');peers.extend([peer,dest]);credentials=struct.unpack('3i',peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12));pid=credentials[0]
 def ident():
  try:return {'pid':pid,'exe':os.readlink(f'/proc/{pid}/exe'),'cmdline':Path(f'/proc/{pid}/cmdline').read_bytes().replace(b'\0',b' ').decode(),'startticks':Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()[19]}
  except OSError:return {'pid':pid}
 report.setdefault('ipc_peers',[]).append({'connection':index,'credentials':credentials,'identity':ident()});buffers={peer:b'',dest:b''}
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
try:
 wait(lambda:Path('/tmp/work/service.sock').exists(),30);client=socket.socket(socket.AF_UNIX);client.settimeout(40);client.connect('/tmp/work/service.sock');wait(lambda:req('status')['status']['ready'],40);req('add_root',path=str(corpus));report['indexed_status']=wait(lambda:(r if (r:=req('status')['status'])['indexed']==1 and r['errors']==1 and not r['busy'] else None),60)
 listener=socket.socket(socket.AF_UNIX);listener.bind('/tmp/work/service-relay.sock');listener.listen();listener.settimeout(.1);at=threading.Thread(target=accept);at.start()
 urllib.request.urlopen(f'http://127.0.0.1:{port}/positive-control').read();report['positive_canary']=list(gets)
 # Uninstrumented stock frontend wire witness; ownership is private D-Bus.
 rr=spawn(['/usr/bin/krunner','--runner','kde-index-runner','find DESKTOP-77331']);wait(lambda:any(e['direction']=='reply' and e['raw'].get('results') for e in events),30);time.sleep(1);rr.terminate();rr.wait(timeout=5);report['uninstrumented_wire_events']=len(events)
 runner=spawn(['/usr/bin/krunner','--runner','kde-index-runner','find DESKTOP-77331'],'runner');time.sleep(2);report['runner_visible_initial']=observe('runner','runner-visible-initial');app=spawn(['/tmp/stage/bin/kde-index','--socket','/tmp/work/service-relay.sock','--models','/tmp/models','--data-dir','/tmp/work/index','--query','DESKTOP-77331'],'app')
 time.sleep(3)
 subprocess.run(['/usr/bin/qdbus6','org.kde.krunner','/App','org.kde.krunner.App.querySingleRunner','kde-index-runner','find DESKTOP-77331'],timeout=5,stdout=subprocess.DEVNULL);time.sleep(1)
 rs=observe('runner','runner-initial');aps=observe('app','app-initial');report['initial_result_nodes']={'runner':find_nodes(rs,lambda n:'TITLE-77331' in n.get('text','') or 'DESKTOP-77331' in n.get('text','')),'app':find_nodes(aps,lambda n:'TITLE-77331' in n.get('text','') or 'DESKTOP-77331' in n.get('text',''))}
 lists=find_nodes(rs,lambda n:n.get('count',0)>0 and n.get('currentIndex') is not None );report['runner_lists']=lists
 if lists:
  delegates=find_nodes(rs,lambda n:n.get('displayText')==title and 'ResultDelegate' in n['class'])
  report['runner_source_delegates']=delegates
  if delegates and lists[0]['currentIndex']!=delegates[0]['index']:
   window=find_nodes(rs,lambda n:n['class']=='View')[0]
   for i in range(3):
    observe('runner','runner-select-down-'+str(i),'key',node=window['node'],key=16777237);time.sleep(.1)
    selected=observe('runner','runner-selected-'+str(i));selected_lists=find_nodes(selected,lambda n:n['class'].startswith('ResultsView') and n.get('count',0)>0)
    if selected_lists and selected_lists[0]['currentIndex']==delegates[0]['index']:break
 time.sleep(.3);rs=observe('runner','runner-current');sub=find_nodes(rs,lambda n:n.get('text','').startswith('DESKTOP-77331 <img') and n.get('effective_visible') and n.get('textFormat')=='PlainText' and n.get('width',0)>0);report['runner_subtext_nodes']=sub
 if sub:observe('runner','runner-hover','hover',node=sub[0]['node']);time.sleep(1.5);report['runner_after_hover']=observe('runner','runner-tooltip');observe('runner','runner-frame','capture')
 # Open the actual indexed UI error dialog, retaining source-derived path/error.
 buttons=find_nodes(aps,lambda n:n.get('text')=='View errors and warnings…')
 if buttons:observe('app','app-errors-open','invoke',node=buttons[0]['node'],method='clicked');time.sleep(.3);report['app_error_nodes']=find_nodes(observe('app','app-errors'),lambda n:'ERROR-77331' in n.get('text','') or 'extractor failed' in n.get('text',''))
 report['privacy_gets_after_visible_consumers']=list(gets)
 # Actual native action dispatch; Dolphin selection read only after async directory load.
 if buttons:
  errors=find_nodes(observe('app','app-errors-dialog'),lambda n:n.get('title')=='Files needing attention')
  if errors:observe('app','app-errors-close','invoke',node=errors[0]['node'],method='close')
 aps=observe('app','app-before-actions')
 reveals=find_nodes(aps,lambda n:n.get('text')=='Show in Dolphin' and n.get('effective_visible'))
 if reveals:
  report['app_reveal_invocation']=observe('app','app-reveal','invoke',node=reveals[0]['node'],method='clicked')['success'];time.sleep(2)
  ds=observe('dolphin','app-reveal-target');report['app_reveal_target']=find_nodes(ds,lambda n:n['class']=='DolphinView')
 if report.get('app_reveal_target'):
  ds=observe('dolphin','close-app-reveal-target');os.kill(ds['snapshot']['pid'],15);time.sleep(.5)
 if lists:
  subprocess.run(['/usr/bin/qdbus6','org.kde.krunner','/App','org.kde.krunner.App.querySingleRunner','kde-index-runner','find DESKTOP-77331'],timeout=5,stdout=subprocess.DEVNULL);time.sleep(1)
  rs=observe('runner','runner-before-reveal');lists=find_nodes(rs,lambda n:n.get('count',0)>0 and n.get('currentIndex') is not None )
  if lists:
   delegates=find_nodes(rs,lambda n:n.get('displayText')==title and 'ResultDelegate' in n['class']);report['runner_action_source_delegates']=delegates
   if not delegates:raise RuntimeError('actual owned source delegate missing')
   window=find_nodes(rs,lambda n:n['class']=='View')[0]
   for i in range(3):
    if lists[0]['currentIndex']==delegates[0]['index']:break
    observe('runner','runner-action-down-'+str(i),'key',node=window['node'],key=16777237);time.sleep(.1)
    selected=observe('runner','runner-action-selected-'+str(i));lists=find_nodes(selected,lambda n:n['class'].startswith('ResultsView') and n.get('count',0)>0)
   if not lists or lists[0]['currentIndex']!=delegates[0]['index']:raise RuntimeError('actual owned source could not be selected by native navigation')
   report['runner_reveal_selected_index']=lists[0]['currentIndex']
   report['runner_reveal_invocation']=observe('runner','runner-reveal','invoke',node=lists[0]['node'],method='runAction',argument=0)['success'];time.sleep(1)
   report['runner_reveal_target']=find_nodes(observe('dolphin','runner-reveal-target'),lambda n:n['class']=='DolphinView')
 if report.get('runner_reveal_target'):
  ds=observe('dolphin','close-runner-reveal-target');os.kill(ds['snapshot']['pid'],15);time.sleep(.5)
 opens=find_nodes(aps,lambda n:n.get('text')=='Open file' and n.get('effective_visible'))
 if opens:
  report['app_open_invocation']=observe('app','app-open','invoke',node=opens[0]['node'],method='clicked')['success'];time.sleep(1)
  try:report['app_open_target']=find_nodes(observe('dolphin','app-open-target'),lambda n:n['class']=='DolphinView')
  except TimeoutError:report['app_open_target_boundary']='No newly observed Dolphin consumer within bounded wait; open not qualified'
  report['app_open_controlled_association']=(O/'config/mimeapps.list').read_text()
 report['status']='frontends-observed';req('shutdown');service.wait(timeout=15)
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
 for proc in reversed(owned):
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=6)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
 http.shutdown();http.server_close();ht.join(2)
 for f in logs:f.close()
 # All additional D-Bus/detached apps are inside this exclusively owned PID namespace.
 extra=[]
 for path in Path('/proc').iterdir():
  if path.name.isdigit() and int(path.name) not in (1,os.getpid(),os.getppid()):
   try:
    pid=int(path.name);cmd=(path/'cmdline').read_bytes().replace(b'\0',b' ').decode();extra.append({'pid':pid,'cmdline':cmd});os.kill(pid,15)
   except (OSError,UnicodeError):pass
 time.sleep(.3)
 report['namespace_extra_processes_closed']=extra
 report['closure']={'owned_exits':[{'pid':p.pid,'exit':p.returncode} for p in owned],'relay_threads_closed':all(not t.is_alive() for t in threads),'canary_thread_closed':not ht.is_alive()}
 (O/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ['status','error','privacy_gets_after_visible_consumers','closure']}));raise SystemExit(0 if report.get('status') else 1)
