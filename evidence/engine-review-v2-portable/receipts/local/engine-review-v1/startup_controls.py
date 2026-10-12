import json,sqlite3,socket,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/engine-review-startup-v1';O.mkdir();R=O/'runtime';R.mkdir();D=R/'mismatched-data';D.mkdir();S=str(R/'socket');events=[];proc=None;conn=None;buf=b'';counter=0;known=set();started=time.monotonic()
def record(label,**x):events.append({'label':label,**x});(O/'events.jsonl').open('a').write(json.dumps(events[-1])+'\n')
def req(op,**kw):
 global counter,buf
 counter+=1;x={'id':str(counter),'op':op,**kw};t=time.monotonic();conn.sendall(json.dumps(x).encode()+b'\n')
 while b'\n' not in buf:
  b=conn.recv(1000000)
  if not b:raise RuntimeError('EOF')
  buf+=b
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==x['id'];record('request',request=x,response=r,seconds=time.monotonic()-t);return r

def children():return {int(v) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in f.read_text().split()}
def launch(data):
 global proc,conn,buf,log
 buf=b'';log=(O/(data.name+'.log')).open('wb');cmd=[str(P/'evidence/engine-v1/candidate/bin/kde-index-service'),'--data-dir',str(data),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')];proc=subprocess.Popen(cmd,stdout=log,stderr=log);record('launch',pid=proc.pid,command=cmd)
 end=time.monotonic()+30
 while time.monotonic()<end:
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(S);conn.settimeout(10);break
  except OSError:time.sleep(.05)
 while time.monotonic()<end:
  s=req('status')['status']
  if s.get('ready') or s.get('error'):known.update(children());return s
  time.sleep(.1)
 raise RuntimeError('startup timeout')
def stop():
 global conn
 req('shutdown');conn.close();conn=None;proc.wait(timeout=15);log.close();record('service-closed',pid=proc.pid,returncode=proc.returncode)
try:
 original=P/'local/engine-review-lifecycle-v2/runtime/data/index.sqlite';src=sqlite3.connect(original);db=sqlite3.connect(D/'index.sqlite');src.backup(db);src.close();v=db.execute("select value from meta where key='identity'").fetchone()[0];identity=json.loads(v);record('before-mismatch',origin=str(original),identity=identity,files=db.execute('select path,error from files').fetchall(),segments=db.execute('select count(*) from segments').fetchone()[0]);identity['engine_pipeline']='deliberately-contradictory-review-identity';db.execute("update meta set value=? where key='identity'",(json.dumps(identity,separators=(',',':')).encode(),));db.commit();db.close()
 record('startup-refusal',status=launch(D));record('wipe-after-refusal',response=req('wipe'));record('scan-after-refusal',response=req('scan'));record('resume-after-refusal',response=req('resume'));record('status-after-controls',response=req('status'))
 db=sqlite3.connect(D/'index.sqlite');record('after-refused-wipe-db',files=db.execute('select path,error from files').fetchall(),segments=db.execute('select count(*) from segments').fetchone()[0],identity=json.loads(db.execute("select value from meta where key='identity'").fetchone()[0]));db.close()
 # Native idle endpoints; no model/tool calls are inside this interval.
 idle_start=time.monotonic();time.sleep(11);idle_end=time.monotonic();record('fatal-idle-child-retention',interval_seconds=idle_end-idle_start,child_pids=sorted(children()));known.update(children());stop()
 fresh=R/'fresh-data';record('external-fresh-dir-startup',status=launch(fresh));record('fresh-dir-wipe',response=req('wipe'));record('fresh-dir-empty-search',response=req('search',text='NEXT-87651'));stop()
except Exception as e:record('error',error=repr(e));raise
finally:
 if conn:conn.close()
 if proc and proc.poll() is None:
  known.update(children());proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
 record('closure',known_children=sorted(known),children_absent=all(not Path(f'/proc/{v}').exists() for v in known),socket_absent=not Path(S).exists(),last_service_returncode=proc.returncode if proc else None,wall_seconds=time.monotonic()-started)
 print(json.dumps(events[-1]))
