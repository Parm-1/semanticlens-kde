import json,socket,sqlite3,struct,subprocess,time
from pathlib import Path
P=Path.cwd();O=P/'local/engine-review-v2-index';O.mkdir();R=O/'runtime';R.mkdir();C=R/'selected';C.mkdir();D=R/'data';S=str(R/'socket');events=[];known=set();proc=None;conn=None;buf=b'';counter=0;started=time.monotonic()
def record(label,**x):
 with (O/'events.jsonl').open('a') as f:f.write(json.dumps({'label':label,**x})+'\n')
def req(op,**kw):
 global counter,buf
 counter+=1;x={'id':str(counter),'op':op,**kw};conn.sendall(json.dumps(x).encode()+b'\n')
 while b'\n' not in buf:
  b=conn.recv(1000000)
  if not b:raise RuntimeError('EOF')
  buf+=b
 line,buf=buf.split(b'\n',1);r=json.loads(line);assert r['id']==x['id'];record('request',request=x,response=r);return r

def children():
 v={int(x) for f in Path(f'/proc/{proc.pid}/task').glob('*/children') for x in f.read_text().split()};known.update(v);return v

def wait(pred,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=req('status')['status'];children()
  if pred(s):return s
  time.sleep(.04)
 raise RuntimeError('timeout '+str(s))
def pdf(path,texts):
 objs=[b'<< /Type /Catalog /Pages 2 0 R >>',None,b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>'];kids=[]
 for text in texts:
  stream=('BT /F1 12 Tf 72 720 Td ('+text+') Tj ET').encode();n=len(objs)+1;kids.append(f'{n} 0 R');objs.append(f'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents {n+1} 0 R >>'.encode());objs.append(b'<< /Length '+str(len(stream)).encode()+b' >>\nstream\n'+stream+b'\nendstream')
 objs[1]=('<< /Type /Pages /Count '+str(len(texts))+' /Kids ['+' '.join(kids)+'] >>').encode();b=b'%PDF-1.4\n';offset=[0]
 for n,obj in enumerate(objs,1):offset.append(len(b));b+=f'{n} 0 obj\n'.encode()+obj+b'\nendobj\n'
 pos=len(b);b+=f'xref\n0 {len(offset)}\n0000000000 65535 f \n'.encode()+b''.join(f'{x:010d} 00000 n \n'.encode() for x in offset[1:]);b+=f'trailer\n<< /Size {len(offset)} /Root 1 0 R >>\nstartxref\n{pos}\n%%EOF\n'.encode();path.write_bytes(b)
try:
 pdf(C/'preview.pdf',['The serial ZXQ 7491 is a near match on the first page.','The serial ZXQ-7491 is the literal on the second page.']);raw=subprocess.run(['pdftotext','-layout','-enc','UTF-8',str(C/'preview.pdf'),'-'],capture_output=True,check=True).stdout;(O/'actual-extracted-pages.txt').write_bytes(raw)
 link=R/'model-link.gguf';link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf');wrong=R/'wrong.gguf';wrong.write_bytes(b'wrong-model-bytes')
 log=(O/'service.log').open('wb');cmd=[str(P/'evidence/engine-v2/candidate/bin/kde-index-service'),'--data-dir',str(D),'--socket',S,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(link),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')];proc=subprocess.Popen(cmd,stdout=log,stderr=log);record('launch',pid=proc.pid,command=cmd)
 end=time.monotonic()+30
 while time.monotonic()<end:
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(S);conn.settimeout(120);break
  except OSError:time.sleep(.05)
 wait(lambda s:s['ready']);req('add_root',path=str(C));wait(lambda s:not s['busy']);r=req('search',text='ZXQ-7491',kind='pdf');record('pdf-page-ranking',response=r)
 qv=json.loads((P/'local/engine-review-v2/producer-responses.jsonl').read_text().splitlines()[0])['vector'];db=sqlite3.connect(D/'index.sqlite');pages=db.execute('select id,path,page,title,text,kind from pages order by id').fetchall();vectors=[]
 for page,page_id,blob in db.execute('select page,page_id,vector from segments order by id'):
  v=struct.unpack('=768f',blob);vectors.append({'page':page,'page_id':page_id,'norm2':sum(x*x for x in v),'dot':sum(a*b for a,b in zip(qv,v))})
 db.close();record('pdf-raw-page-vector-association',pages=pages,vectors=vectors,query_vector_reference='local/engine-review-v2/producer-responses.jsonl native-query')
 # Establish ordinary idle closure, then exercise indexing as the first reload caller.
 idle_start=time.monotonic();end=idle_start+20
 while time.monotonic()<end and children():time.sleep(.1)
 record('ordinary-idle',elapsed_seconds=time.monotonic()-idle_start,children=sorted(children()));assert not children();link.unlink();link.symlink_to(wrong);(C/'new-failure.txt').write_text('No accepted row may be fabricated for this failure.');record('scan-scheduling-ack',response=req('scan'));s=wait(lambda s:bool(s['error']));record('indexing-reload-failure-status',status=s,children=sorted(children()))
 db=sqlite3.connect(D/'index.sqlite');record('indexing-reload-failure-db',files=db.execute('select path,error from files order by path').fetchall(),pages=db.execute('select page,title,text from pages order by id').fetchall(),vectors=db.execute('select count(*) from segments').fetchone()[0],failing_file_rows=db.execute('select path from files where path=?',(str(C/'new-failure.txt'),)).fetchall());db.close()
 link.unlink();link.symlink_to(P/'models/embeddinggemma-2-Q8_0.gguf');record('indexing-recovery-resume',response=req('resume'));wait(lambda s:s['ready'] and not s['busy']);record('indexing-recovery-query',response=req('search',text='No accepted row'));req('shutdown');conn.close();conn=None;proc.wait(timeout=15)
except Exception as e:record('error',error=repr(e));raise
finally:
 if conn:conn.close()
 if proc:
  known.update(children())
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 record('closure',service_returncode=proc.returncode if proc else None,known_children=sorted(known),children_absent=all(not Path(f'/proc/{v}').exists() for v in known),socket_absent=not Path(S).exists(),wall_seconds=time.monotonic()-started)
 print('done')
