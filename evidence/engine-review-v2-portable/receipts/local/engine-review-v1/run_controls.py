import hashlib,json,os,socket,sqlite3,struct,subprocess,time
from pathlib import Path
P=Path.cwd(); O=P/'local/engine-review-v1';R=O/'runtime';R.mkdir();C=R/'selected';C.mkdir();D=R/'data';S=str(R/'socket');events=[];known=set();start=time.monotonic();proc=None;conn=None;counter=0;buffer=b''
def record(label,**x):events.append({'label':label,**x})
def req(op,**kw):
 global counter,buffer
 counter+=1;x={'id':str(counter),'op':op,**kw};t=time.monotonic();conn.sendall(json.dumps(x).encode()+b'\n')
 while b'\n' not in buffer:
  b=conn.recv(1000000)
  if not b:raise RuntimeError('unexpected EOF')
  buffer+=b
 line,buffer=buffer.split(b'\n',1);r=json.loads(line);assert r['id']==x['id'];record('request',request=x,response=r,seconds=time.monotonic()-t);return r

def wait(pred,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=req('status')['status']
  if pred(s):return s
  if s.get('error'):raise RuntimeError(s['error'])
  time.sleep(.1)
 raise RuntimeError('timeout '+str(s))
def children():
 return {int(v) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in f.read_text().split()}
def db_snapshot(label):
 db=sqlite3.connect(D/'index.sqlite');rows=[]
 for p,title,text,page,kind,blob in db.execute('select path,title,text,page,kind,vector from segments order by id'):
  vec=struct.unpack('=768f',blob);rows.append({'path':p,'title':title,'text':text,'page':page,'kind':kind,'vector_sha256':hashlib.sha256(blob).hexdigest(),'vector_norm2':sum(v*v for v in vec)})
 files=db.execute('select path,hash,error,warning from files order by path').fetchall();meta=db.execute('select key,value from meta order by key').fetchall();db.close();record(label,files=files,segments=rows,meta=meta);return rows
try:
 for i in range(5):(C/f'a{i}-near.txt').write_text('Inventory serial ZXQ 7491 is a punctuation near-match.')
 (C/'z-literal.txt').write_text('Inventory serial ZXQ-7491 is the deliberately unique literal identifier.')
 (C/'boundary.txt').write_text('x '*898+' '+'ZXQ-7491'+' is the deliberately boundary-spanning identifier.')
 outside=R/'outside';outside.mkdir();(outside/'outside.txt').write_text('OUTSIDE-75421')
 (C/'outside-link.txt').symlink_to(outside/'outside.txt');(C/'outside-dir').symlink_to(outside,target_is_directory=True)
 for p in ['.hidden','secrets','build-generated']:
  (C/p).mkdir();(C/p/'private.txt').write_text('PRIVATE-92354')
 (C/'credentials.txt').write_text('PRIVATE-92354')
 model_link=R/'text-model-link.gguf';model_link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf')
 command=[str(P/'evidence/engine-v1/candidate/bin/kde-index-service'),'--data-dir',str(D),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(model_link),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')]
 log=(O/'service.log').open('wb');proc=subprocess.Popen(command,stdout=log,stderr=log);record('launch',pid=proc.pid,command=command)
 end=time.monotonic()+30
 while time.monotonic()<end:
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(S);conn.settimeout(120);break
  except OSError:time.sleep(.05)
 wait(lambda s:s['ready']);known.update(children());record('no-roots-positive',response=req('status'))
 req('add_root',path=str(C));wait(lambda s:not s['busy']);rows=db_snapshot('initial-db')
 record('literal-vs-fts',response=req('search',text='ZXQ-7491',limit=5))
 # Independently ask the actual pinned producer for the query and literal document framing.
 requests=[{'id':'native-query','op':'query','text':'ZXQ-7491'},{'id':'native-document','op':'document','title':'z-literal.txt','text':(C/'z-literal.txt').read_text()}]
 (O/'producer-requests.jsonl').write_text(''.join(json.dumps(x)+'\n' for x in requests))
 plog=(O/'producer.log').open('wb');native=subprocess.Popen([str(P/'build-probe/embedding-probe'),str(P/'models/embeddinggemma-2-Q8_0.gguf'),str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'6'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=plog)
 stdout,_=native.communicate((O/'producer-requests.jsonl').read_bytes(),timeout=40);plog.close();(O/'producer-responses.jsonl').write_bytes(stdout);record('native-closure',pid=native.pid,returncode=native.returncode)
 nr=[json.loads(x) for x in stdout.splitlines()];qv=nr[0]['vector'];dv=nr[1]['vector']
 db=sqlite3.connect(D/'index.sqlite');blob=db.execute("select vector from segments where title='z-literal.txt'").fetchone()[0];db.close();stored=struct.unpack('=768f',blob);native_bytes=struct.pack('=768f',*dv)
 record('independent-producer-vector',stored_equals_native=blob==native_bytes,raw_dot=sum(a*b for a,b in zip(qv,stored)),native_document_sha256=hashlib.sha256(native_bytes).hexdigest())
 # Boundary input contains the literal but neither accepted passage does.
 boundary=[r for r in rows if r['title']=='boundary.txt'];record('boundary-raw-fields',source_contains_literal='ZXQ-7491' in (C/'boundary.txt').read_text(),segments_contain_literal=['ZXQ-7491' in r['text'] for r in boundary],segment_lengths=[len(r['text']) for r in boundary])
 # Current producer idle exit is observed, then only an owned model-path link is redirected.
 end=time.monotonic()+20
 while time.monotonic()<end and children():time.sleep(.1)
 record('idle-child-closure',remaining=list(children()));assert not children()
 wrong=R/'wrong.gguf';wrong.write_bytes(b'wrong-model-bytes');model_link.unlink();model_link.symlink_to(wrong)
 record('reload-wrong-model',response=req('search',text='ZXQ-7491'))
 record('status-after-reload-failure',response=req('status'));db_snapshot('db-after-reload-failure')
 model_link.unlink();model_link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf');record('reload-recovery',response=req('search',text='ZXQ-7491',limit=1));known.update(children())
 # Exclusion acknowledgement is an actual negative at the applicable accepted phase.
 record('exclude-ack',response=req('set_exclusions',path=str(C),exclusions=['z-literal.txt']))
 record('excluded-negative',response=req('search',text='ZXQ-7491'));wait(lambda s:not s['busy']);db_snapshot('after-exclusion-db')
 record('wipe-ack',response=req('wipe'));record('wipe-negative',response=req('search',text='ZXQ-7491'));db_snapshot('after-wipe-db')
 req('shutdown');conn.close();conn=None;proc.wait(timeout=15)
except Exception as e:
 record('error',error=repr(e));raise
finally:
 if conn:conn.close()
 if proc:
  known.update(children())
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 record('closure',service_returncode=proc.returncode if proc else None,known_children=sorted(known),children_absent=all(not Path(f'/proc/{p}').exists() for p in known),socket_absent=not Path(S).exists(),wall_seconds=time.monotonic()-start,usage='unknown; runner records native wall time only')
 (O/'raw-controls.json').write_text(json.dumps(events,indent=2)+'\n')
 print(json.dumps(events[-1]))
