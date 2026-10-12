import json,socket,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/engine-review-v2-commit';O.mkdir();R=O/'runtime';R.mkdir();D=R/'data';C=R/'selected';C.mkdir();(C/'source.txt').write_text('COMMIT-92354 real selected source');S=str(R/'socket');events=[];known=set();proc=None;clients=[];started=time.monotonic()
def record(label,**kw):
 with (O/'events.jsonl').open('a') as f:f.write(json.dumps({'label':label,**kw})+'\n')
class Client:
 def __init__(self):self.s=socket.socket(socket.AF_UNIX);self.s.connect(S);self.s.settimeout(30);self.buf=b'';self.count=0;clients.append(self)
 def send(self,op,**kw):
  self.count+=1;x={'id':str(self.count),'op':op,**kw};self.s.sendall(json.dumps(x).encode()+b'\n');record('send',request=x);return x['id']
 def recv(self):
  while b'\n' not in self.buf:
   b=self.s.recv(1000000)
   if not b:raise RuntimeError('EOF')
   self.buf+=b
  line,self.buf=self.buf.split(b'\n',1);return json.loads(line)
 def req(self,op,**kw):
  t=time.monotonic();i=self.send(op,**kw);r=self.recv();assert r['id']==i;record('reply',response=r,seconds=time.monotonic()-t);return r

def children():
 v={int(x) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for x in f.read_text().split()};known.update(v);return v
try:
 log=(O/'service.log').open('wb');cmd=[str(P/'evidence/engine-v2/candidate/bin/kde-index-service'),'--data-dir',str(D),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')];proc=subprocess.Popen(cmd,stdout=log,stderr=log);record('launch',pid=proc.pid,command=cmd)
 end=time.monotonic()+30
 while time.monotonic()<end:
  try:owner=Client();observer=Client();break
  except OSError:time.sleep(.05)
 while time.monotonic()<end:
  s=owner.req('status')['status'];children()
  if s['ready']:break
  time.sleep(.05)
 owner.req('add_root',path=str(C))
 while owner.req('status')['status']['busy']:time.sleep(.03)
 before=owner.req('status')['status'];children();wipe=owner.send('wipe');observations=[];end=time.monotonic()+20
 while time.monotonic()<end:
  t=time.monotonic();s=observer.req('status')['status'];observations.append({'status':s,'seconds':time.monotonic()-t});children()
  if s.get('phase') in ['wiping','verifying'] and not s['ready']:break
  time.sleep(.003)
 else:raise RuntimeError('post-acceptance wipe phase not observed')
 cancel=owner.send('cancel',target=wipe);got={};t=time.monotonic()
 while len(got)<2:
  r=owner.recv();got[r['id']]=r;record('pending-reply',response=r,seconds_since_cancel_send=time.monotonic()-t)
 record('post-acceptance-wipe-cancel',before_status=before,observed_phase_status=s,wipe=got[wipe],cancel=got[cancel],observer_samples=observations)
 assert got[cancel]['accepted'] is False and got[wipe]['removed'] is True and got[wipe]['ready'] is True
 after=observer.req('status');record('after-status',response=after,source_text=(C/'source.txt').read_text());owner.req('shutdown');proc.wait(timeout=15)
except Exception as e:record('error',error=repr(e));raise
finally:
 for c in clients:c.s.close()
 if proc:
  known.update(children())
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 record('closure',service_returncode=proc.returncode if proc else None,known_children=sorted(known),children_absent=all(not Path(f'/proc/{v}').exists() for v in known),socket_absent=not Path(S).exists(),wall_seconds=time.monotonic()-started)
 print('done')
