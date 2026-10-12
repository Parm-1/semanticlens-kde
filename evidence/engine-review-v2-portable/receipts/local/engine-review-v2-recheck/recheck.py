import hashlib,json,socket,sqlite3,struct,subprocess,time,threading,os
from pathlib import Path
P=Path.cwd();O=P/'local/engine-review-v2-recheck';R=O/'runtime';R.mkdir();C=R/'selected';C.mkdir();D=R/'data';S=str(R/'socket');events=[];known=set();procs=[];conn=None;buf=b'';counter=0;started=time.monotonic();producer=None

def record(label,**x):
 r={'label':label,**x};events.append(r)
 with (O/'events.jsonl').open('a') as f:f.write(json.dumps(r)+'\n')
def req(op,**kw):
 global counter
 counter+=1;x={'id':str(counter),'op':op,**kw};t=time.monotonic();conn.sendall(json.dumps(x).encode()+b'\n');r=recv();assert r['id']==x['id'],(x,r);record('request',request=x,response=r,seconds=time.monotonic()-t);return r

def recv():
 global buf
 while b'\n' not in buf:
  b=conn.recv(1000000)
  if not b:raise RuntimeError('EOF')
  buf+=b
 line,buf=buf.split(b'\n',1);return json.loads(line)
def children():
 v={int(x) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for x in f.read_text().split()};known.update(v);return v

def wait(pred,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=req('status')['status'];children()
  if pred(s):return s
  time.sleep(.04)
 raise RuntimeError('timeout '+str(s))
def launch():
 global proc,conn,buf
 buf=b'';log=(O/f'service-{len(procs)}.log').open('wb');cmd=[str(P/'evidence/engine-v2/candidate/bin/kde-index-service'),'--data-dir',str(D),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(model_link),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')];proc=subprocess.Popen(cmd,stdout=log,stderr=log);procs.append((proc,log));record('launch',pid=proc.pid,command=cmd)
 end=time.monotonic()+30
 while time.monotonic()<end:
  if proc.poll() is not None:raise RuntimeError('startup exit')
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(S);conn.settimeout(120);break
  except OSError:time.sleep(.05)
 return wait(lambda s:s['ready'] or bool(s['error']))
def stop():
 global conn
 req('shutdown');conn.close();conn=None;proc.wait(timeout=15);record('service-closed',pid=proc.pid,returncode=proc.returncode)

def dbread(label):
 db=sqlite3.connect(D/'index.sqlite');rows=[]
 for sid,path,pid,title,text,page,kind,blob in db.execute('select id,path,page_id,title,text,page,kind,vector from segments order by id'):
  v=struct.unpack('=768f',blob);rows.append({'id':sid,'path':path,'page_id':pid,'title':title,'text':text,'page':page,'kind':kind,'vector_sha256':hashlib.sha256(blob).hexdigest(),'vector_norm2':sum(x*x for x in v)})
 pages=db.execute('select id,path,page,title,text,kind from pages order by id').fetchall();files=db.execute('select path,hash,error,warning from files order by path').fetchall();meta=[(k,v.decode() if isinstance(v,bytes) else v) for k,v in db.execute('select key,value from meta order by key')];db.close();record(label,segments=rows,pages=pages,files=files,meta=meta);return rows,pages,meta

def pipeline_pause_cancel():
 conn.sendall(b'{"id":"pipeline-pause","op":"pause"}\n{"id":"pipeline-cancel","op":"cancel","target":"pipeline-pause"}\n')
 replies=[recv(),recv()];record('pipeline-pause-cancel',responses=replies);return {r['id']:r for r in replies}
def redirect(path):model_link.unlink();model_link.symlink_to(path)

try:
 for i in range(5):(C/f'a{i}-near.txt').write_text('Inventory serial ZXQ 7491 is a punctuation near-match.')
 (C/'z-literal.txt').write_text('Inventory serial ZXQ-7491 is the deliberately unique literal identifier.')
 (C/'boundary.txt').write_text('x '*898+' '+'ZXQ-7491'+' is the deliberately boundary-spanning identifier.')
 outside=R/'outside';outside.mkdir();(outside/'outside.txt').write_text('OUTSIDE-75421');(C/'outside-link.txt').symlink_to(outside/'outside.txt')
 model_link=R/'text-model-link.gguf';model_link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf');wrong=R/'wrong.gguf';wrong.write_bytes(b'wrong-model-bytes')
 record('initial-ready',status=launch());req('add_root',path=str(C));wait(lambda s:not s['busy']);rows,pages,meta=dbread('initial-accepted-db')
 literal=req('search',text='ZXQ-7491',limit=5);record('original-ranking-falsifier',response=literal)
 lower=req('search',text='zxq-7491',limit=5);record('case-insensitive-literal',response=lower)
 record('kind-negative',response=req('search',text='ZXQ-7491',kind='pdf'))
 # Reuse the independently produced unchanged query vector, and calculate dots from raw current stored vectors.
 native=json.loads((P/'local/engine-review-v2/producer-responses.jsonl').read_text().splitlines()[0])['vector']
 db=sqlite3.connect(D/'index.sqlite');vectors={}
 for title,page,pid,blob in db.execute('select title,page,page_id,vector from segments'):
  v=struct.unpack('=768f',blob);vectors.setdefault((title,page),[]).append({'page_id':pid,'vector_sha256':hashlib.sha256(blob).hexdigest(),'dot':sum(a*b for a,b in zip(native,v))})
 db.close();record('independent-raw-dot-and-page-association',producer_reference='local/engine-review-v2/producer-responses.jsonl native-query',stored_vectors=[{'title':k[0],'page':k[1],'vectors':v} for k,v in vectors.items()],returned=[{'title':r['title'],'page':r['page'],'semantic_score':r['semantic_score'],'excerpt_is_source_substring':r['text'] in (C/r['title']).read_text(),'complete_literal':('ZXQ-7491' in r['text'])} for r in literal['results']])
 # Concurrent cheap status while valid commit/control and cancellation are exercised on the sole writer.
 (C/'a-big.txt').write_text('x '*9000);req('scan');wait(lambda s:s['busy'] and s['current'].endswith('a-big.txt'))
 got=pipeline_pause_cancel();record('after-canceled-pause-status',response=req('status'));dbread('after-canceled-pause-db')
 record('invalid-control',response=req('set_exclusions',path=str(C),exclusions=['../outside']))
 wait(lambda s:not s['busy']);dbread('scan-after-rejected-controls')
 (C/'a-big.txt').write_text('Changed delivery schedule and repair content. '*500);req('scan');wait(lambda s:s['busy'] and s['current'].endswith('a-big.txt'))
 record('valid-pause',response=req('pause'));record('valid-pause-status',response=req('status'));dbread('valid-pause-db');record('late-cancel',response=req('cancel',target=str(counter-2)));req('resume');wait(lambda s:not s['busy']);dbread('resume-complete-db')
 # Status observer uses another socket, and raw timings are retained without target-p95 interpretation.
 stop_poll=threading.Event();poll=[]
 def poll_status():
  c=socket.socket(socket.AF_UNIX);c.connect(S);c.settimeout(20);b=b'';i=0
  try:
   while not stop_poll.is_set():
    i+=1;t=time.monotonic();c.sendall(json.dumps({'id':'observer-'+str(i),'op':'status'}).encode()+b'\n')
    while b'\n' not in b:
     chunk=c.recv(1000000)
     if not chunk:raise RuntimeError('observer EOF')
     b+=chunk
    line,b=b.split(b'\n',1);poll.append({'response':json.loads(line),'seconds':time.monotonic()-t});time.sleep(.015)
  except Exception as e:poll.append({'error':repr(e)})
  finally:c.close()
 thread=threading.Thread(target=poll_status);thread.start()
 (C/'commit.txt').write_text('Commit acceptance control content. '*140);req('scan');wait(lambda s:not s['busy']);record('exclusion-accept',response=req('set_exclusions',path=str(C),exclusions=['z-literal.txt']));record('after-exclusion-search',response=req('search',text='ZXQ-7491'));wait(lambda s:not s['busy']);stop_poll.set();thread.join(25);assert not thread.is_alive();record('concurrent-status-observer',samples=poll)
 wait(lambda s:not s['busy']);idle_start=time.monotonic();end=idle_start+20
 while time.monotonic()<end and children():time.sleep(.1)
 record('ordinary-idle-closure',elapsed_seconds=time.monotonic()-idle_start,remaining=sorted(children()));assert not children()
 redirect(wrong);record('bad-reload-search',response=req('search',text='ZXQ-7491'));record('bad-reload-status',response=req('status'));record('bad-reload-children',children=sorted(children()));dbread('bad-reload-db')
 record('failed-verification-wipe',response=req('wipe'));record('failed-verification-wipe-status',response=req('status'));record('failed-verification-wipe-files',database_present=(D/'index.sqlite').exists(),source_literal_unchanged=(C/'z-literal.txt').read_text()=='Inventory serial ZXQ-7491 is the deliberately unique literal identifier.')
 redirect(P/'models/embeddinggemma-2-Q8_0.gguf');record('deliberate-resume-recovery',response=req('resume'));wait(lambda s:s['ready'] and not s['busy']);dbread('fresh-reverified-db');req('add_root',path=str(C));wait(lambda s:not s['busy']);record('pre-incompatibility-status',response=req('status'));stop()
 # Actual accepted v2 rows; contradiction changes only the owned persisted extractor identity.
 db=sqlite3.connect(D/'index.sqlite');v=json.loads(db.execute("select value from meta where key='identity'").fetchone()[0]);v['extractors']['pdftotext']='deliberately-contradictory-review-version';db.execute("update meta set value=? where key='identity'",(json.dumps(v,separators=(',',':')).encode(),));db.commit();db.close()
 record('incompatible-startup',status=launch());record('incompatible-children',children=sorted(children()));t1=time.monotonic();time.sleep(11);t2=time.monotonic();record('fatal-idle-endpoints',interval_seconds=t2-t1,remaining=sorted(children()));dbread('incompatible-retained-db')
 record('incompatible-wipe',response=req('wipe'));record('post-incompatible-wipe-status',response=req('status'));dbread('post-incompatible-wipe-db');stop()
 # Partial owned-file removal failure, closed retry, and source/sentinel preservation.
 (D/'index.sqlite-shm').mkdir();(D/'owned-sentinel.txt').write_text('keep this unrelated data-dir file')
 record('partial-fixture-startup',status=launch());record('partial-removal-wipe',response=req('wipe'));record('partial-removal-status',response=req('status'));record('partial-removal-files',database_present=(D/'index.sqlite').exists(),shm_directory=(D/'index.sqlite-shm').is_dir(),sentinel_text=(D/'owned-sentinel.txt').read_text(),source_literal=(C/'z-literal.txt').read_text());(D/'index.sqlite-shm').rmdir();record('partial-removal-retry',response=req('wipe'));record('partial-removal-retry-status',response=req('status'));stop()
except Exception as e:
 record('error',error=repr(e));raise
finally:
 if conn:conn.close()
 for p,log in procs:
  if p.poll() is None:p.terminate()
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  log.close()
 record('closure',services=[{'pid':p.pid,'returncode':p.returncode} for p,_ in procs],known_children=sorted(known),children_absent=all(not Path(f'/proc/{p}').exists() for p in known),socket_absent=not Path(S).exists(),wall_seconds=time.monotonic()-started,usage='unknown; native whole-run wall scope only')
 print(json.dumps(events[-1]))
