import json,socket,sqlite3,subprocess,time,os
from pathlib import Path
P=Path.cwd();O=P/'local/engine-review-lifecycle-v1';O.mkdir();R=O/'runtime';R.mkdir();C=R/'selected';C.mkdir();D=R/'data';S=str(R/'socket');events=[];buf=b'';counter=0;proc=None;conn=None;known=set();started=time.monotonic()
def record(label,**x):events.append({'label':label,**x});(O/'events.jsonl').open('a').write(json.dumps(events[-1])+'\n')
def req(op,**kw):
 global counter,buf
 counter+=1;x={'id':str(counter),'op':op,**kw};conn.sendall(json.dumps(x).encode()+b'\n')
 while b'\n' not in buf:
  b=conn.recv(1000000)
  if not b:raise RuntimeError('EOF')
  buf+=b
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==x['id'];record('request',request=x,response=r);return r

def wait(pred,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=req('status')['status']
  if pred(s):return s
  if s.get('error'):raise RuntimeError(s['error'])
  time.sleep(.03)
 raise RuntimeError('timeout '+str(s))
def children():return {int(v) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in f.read_text().split()}
def database(label):
 db=sqlite3.connect(D/'index.sqlite');record(label,files=db.execute('select path,error from files').fetchall(),meta=[(k,v.decode() if isinstance(v,bytes) else v) for k,v in db.execute('select key,value from meta')]);db.close()
try:
 (C/'a-big.txt').write_text('x '*180000);(C/'z-later.txt').write_text('NEXT-87651 should be indexed after the preceding file.')
 log=(O/'service.log').open('wb');cmd=[str(P/'evidence/engine-v1/candidate/bin/kde-index-service'),'--data-dir',str(D),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')];proc=subprocess.Popen(cmd,stdout=log,stderr=log);record('launch',pid=proc.pid,command=cmd)
 end=time.monotonic()+30
 while time.monotonic()<end:
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(S);conn.settimeout(120);break
  except OSError:time.sleep(.05)
 wait(lambda s:s['ready']);known.update(children());req('add_root',path=str(C));wait(lambda s:s['busy'] and s['current'].endswith('a-big.txt'))
 temporary=socket.socket(socket.AF_UNIX);temporary.connect(S);temporary.sendall(b'{"id":"disconnected-pause","op":"pause"}\n');wait(lambda s:s['paused']);temporary.close();time.sleep(.8);database('after-canceled-pause');record('after-canceled-pause-status',response=req('status'))
 # Recovery is explicit; remove the large file only within this owned synthetic root.
 (C/'a-big.txt').unlink();record('resume-recovery',response=req('resume'));wait(lambda s:not s['busy']);record('recovered-search',response=req('search',text='NEXT-87651'));database('after-resume')
 # Invalid control during new in-flight work invalidates its scan although the mutation is rejected.
 (C/'a-big.txt').write_text('y '*180000);req('scan');wait(lambda s:s['busy'] and s['current'].endswith('a-big.txt'));record('invalid-control-during-scan',response=req('set_exclusions',path=str(C),exclusions=['../outside']));wait(lambda s:not s['busy']);record('after-invalid-control-status',response=req('status'));database('after-invalid-control-db')
 # Explicit recovery shows accepted unchanged-root state can finish after the rejected request.
 (C/'a-big.txt').unlink();req('scan');wait(lambda s:not s['busy']);req('shutdown');conn.close();conn=None;proc.wait(timeout=15)
except Exception as e:record('error',error=repr(e));raise
finally:
 if conn:conn.close()
 if proc:
  known.update(children())
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 record('closure',returncode=proc.returncode if proc else None,known_children=sorted(known),children_absent=all(not Path(f'/proc/{v}').exists() for v in known),socket_absent=not Path(S).exists(),wall_seconds=time.monotonic()-started)
 print(json.dumps(events[-1]))
