#!/usr/bin/env python3
"""Bounded actual-worker integration. Writes only synthetic local/engine-* and new evidence stem."""
import argparse, hashlib, json, os, shutil, socket, sqlite3, subprocess, time, struct
from pathlib import Path
P=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',default='local/engine-integration');p.add_argument('--project-root');p.add_argument('--service',default='build-engine/kde-index-service');p.add_argument('--cli',default='build-engine/kde-index-cli');args=p.parse_args();P=Path(args.project_root).resolve() if args.project_root else P
SERVICE=P/args.service;CLI=P/args.cli
out=P/args.output
if out.exists():raise SystemExit('output already exists; preserve old receipt')
out.mkdir(parents=True);runtime=out/'runtime';corpus=runtime/'corpus';data=runtime/'data';corpus.mkdir(parents=True)
results={'contract':'native actual CPU engine integration; diagnostic corpus, no release recall claim','checks':[],'responses':[],'request_wall_seconds':{},'measurement_scope':'monotonic client send through decoded matching response, actual CPU; includes cold/warm requests; not target latency benchmark','model_usage':'unknown; billing/agent token metrics not exposed by runner'}
processes=[];counter=0;conn=None;known_workers=set();started=time.monotonic()
sock_path='/tmp/kde-index-test-'+str(os.getpid())
def check(name,value,detail=None):
 results['checks'].append({'name':name,'pass':bool(value),'detail':detail})
 if not value:raise AssertionError(name+': '+str(detail))
def start(extra=None):
 global conn
 log=(out/('service-'+str(len(processes))+'.log')).open('wb')
 command=[str(SERVICE),'--data-dir',str(data),'--socket',sock_path,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')]
 command+=['--probe-sha256',hashlib.sha256((P/'build-probe/embedding-probe').read_bytes()).hexdigest()]
 if extra:command+=extra
 proc=subprocess.Popen(command,stdout=log,stderr=log);processes.append((proc,log));deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  if proc.poll() is not None:raise RuntimeError('service exited')
  try:conn=socket.socket(socket.AF_UNIX);conn.connect(sock_path);conn.settimeout(120);break
  except OSError:time.sleep(.05)
 wait(lambda s:s['ready'],30)
 known_workers.update(int(v) for x in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in x.read_text().split())
 return proc
buffer=b''
def send(op,**kw):
 global counter
 counter+=1;r={'id':str(counter),'op':op,**kw};conn.sendall(json.dumps(r).encode()+b'\n');return str(counter)
def receive(target):
 global buffer
 while True:
  while b'\n' not in buffer:buffer+=conn.recv(65536)
  line,buffer=buffer.split(b'\n',1);r=json.loads(line)
  results['responses'].append(r)
  if r['id']==target:return r
  raise AssertionError('unexpected out of order response '+str(r))
def request(op,**kw):
 begin=time.monotonic();r=receive(send(op,**kw));results['request_wall_seconds'].setdefault(op,[]).append(time.monotonic()-begin);return r
def status():return request('status')['status']
def wait(predicate,seconds=90):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  s=status()
  if s.get('error'):raise RuntimeError(s['error'])
  if predicate(s):return s
  time.sleep(.1)
 raise AssertionError('wait timed out '+str(s))
def idle():return wait(lambda s:not s['busy'])
def startup_error(seconds=20):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  s=status()
  if s['error']:return s
  time.sleep(.1)
 raise AssertionError('expected startup error not observed')
def search(text,**kw):
 r=request('search',text=text,**kw);check('search-protocol-'+text,r['ok'],r.get('error'));return r['results']
def stop():
 global conn,buffer
 request('shutdown');conn.close();conn=None;buffer=b'';processes[-1][0].wait(timeout=15)
def pdf(path,pages):
 objects=[b'<< /Type /Catalog /Pages 2 0 R >>',None,b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>'];kids=[]
 for text in pages:
  stream=('BT /F1 12 Tf 72 720 Td ('+text.replace('\\','\\\\').replace('(','\\(').replace(')','\\)')+') Tj ET').encode();n=len(objects)+1;kids.append(f'{n} 0 R')
  objects.append(f'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents {n+1} 0 R >>'.encode());objects.append(b'<< /Length '+str(len(stream)).encode()+b' >>\nstream\n'+stream+b'\nendstream')
 objects[1]=('<< /Type /Pages /Count '+str(len(kids))+' /Kids ['+' '.join(kids)+'] >>').encode();b=b'%PDF-1.4\n';offsets=[0]
 for n,o in enumerate(objects,1):offsets.append(len(b));b+=f'{n} 0 obj\n'.encode()+o+b'\nendobj\n'
 pos=len(b);b+=f'xref\n0 {len(offsets)}\n0000000000 65535 f \n'.encode()+b''.join(f'{v:010d} 00000 n \n'.encode() for v in offsets[1:]);b+=f'trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{pos}\n%%EOF\n'.encode();path.write_bytes(b)
def scanned_pdf(path):
 source=runtime/'ocr-source.pdf';pdf(source,['SCANNED 77441 maintenance archive'])
 subprocess.run(['pdftoppm','-singlefile','-scale-to','1200',str(source),str(runtime/'raster')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 ppm=(runtime/'raster.ppm').read_bytes();header,pixels=ppm.split(b'\n255\n',1);lines=header.splitlines();width,height=map(int,lines[-1].split())
 stream=b'q 612 0 0 792 0 0 cm /Im1 Do Q'
 objects=[b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Count 1 /Kids [3 0 R] >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /XObject << /Im1 4 0 R >> >> /Contents 5 0 R >>',f'<< /Type /XObject /Subtype /Image /Width {width} /Height {height} /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length {len(pixels)} >>'.encode()+b'\nstream\n'+pixels+b'\nendstream',f'<< /Length {len(stream)} >>'.encode()+b'\nstream\n'+stream+b'\nendstream']
 b=b'%PDF-1.4\n';offsets=[0]
 for n,o in enumerate(objects,1):offsets.append(len(b));b+=f'{n} 0 obj\n'.encode()+o+b'\nendobj\n'
 pos=len(b);b+=f'xref\n0 {len(offsets)}\n0000000000 65535 f \n'.encode()+b''.join(f'{v:010d} 00000 n \n'.encode() for v in offsets[1:]);b+=f'trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{pos}\n%%EOF\n'.encode();path.write_bytes(b)
try:
 fixture=json.loads((P/'evidence/embedding-v1/inputs/retrieval.json').read_text())
 for item in fixture['corpus']:
  if 'text' in item:(corpus/(item['id']+'.txt')).write_text(item['text'])
  else:shutil.copyfile(P/'evidence/embedding-v1/inputs'/item['image'],corpus/('untagged-'+item['id']+Path(item['image']).suffix))
 pdf(corpus/'manual.pdf',['The front desk stores library membership forms.','Maintenance serial PDF-10299 belongs to the cooling system.'])
 scanned_pdf(corpus/'scanned.pdf')
 (corpus/'broken.png').write_bytes(b'not an image')
 (corpus/'secrets.txt').write_text('DO-NOT-INDEX-8821')
 for dirname in ['.hidden','credentials','build','build-generated']:
  (corpus/dirname).mkdir();(corpus/dirname/'secret.txt').write_text('HIDDEN-99221 confidential marker')
 (corpus/'excluded').mkdir();(corpus/'excluded'/'skip.txt').write_text('EXCLUDE-10101')
 outside=runtime/'outside';outside.mkdir();(outside/'outside.txt').write_text('OUTSIDE-12121')
 (corpus/'link.txt').symlink_to(outside/'outside.txt');(corpus/'linked-dir').symlink_to(outside,target_is_directory=True)
 proc=start();check('no-default-roots',status()['roots']==[])
 contender=subprocess.run([str(SERVICE),'--data-dir',str(data),'--socket',sock_path+'-other','--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')],capture_output=True,timeout=5);check('single-database-writer-lock',contender.returncode==2)
 cli=subprocess.run([str(CLI),'--socket',sock_path,'{"op":"status"}'],capture_output=True,text=True,timeout=5);check('usable-cli',cli.returncode==0 and json.loads(cli.stdout)['protocol']==1)
 check('reject-engine-data-root',not request('add_root',path=str(data))['ok'])
 check('add-root',request('add_root',path=str(corpus))['ok']);latencies=[]
 for _ in range(15):
  begin=time.monotonic();s=status();latencies.append(time.monotonic()-begin);time.sleep(.04)
 check('status-responsive-during-indexing',max(latencies)<.25,{'max_seconds':max(latencies),'observed_busy':any(r.get('status',{}).get('busy') for r in results['responses'])})
 s=idle();check('actual-file-count',s['indexed']==11 and s['errors']==1,s)
 conn_db=sqlite3.connect(data/'index.sqlite');rows=conn_db.execute('select f.path,f.error,count(s.id) from files f left join segments s on f.path=s.path group by f.path').fetchall();conn_db.close();check('failed-image-no-vector',any(Path(path).name=='broken.png' and error and n==0 for path,error,n in rows));check('privacy-symlink-hidden-generated',not any('/.hidden/' in path or '/credentials/' in path or '/build/' in path or 'link' in Path(path).name or Path(path).name=='secrets.txt' or '/build-generated/' in path for path,_,_ in rows))
 for q in fixture['queries']:
  if not q['relevant']:continue
  rr=search(q['text']);wanted=q['relevant'][0];found=[Path(r['path']).stem for r in rr[:5]];check('diagnostic-top-five-'+q['id'],any(v==wanted or v=='untagged-'+wanted for v in found),found)
 absent=search('An invoice for a submarine repair');results['no_match_diagnostic']={'returned':len(absent),'abstention':'not implemented; semantic candidates remain'}
 exact=search('ZXQ-7491');check('identifier-top-one',Path(exact[0]['path']).name=='identifier.txt' and exact[0]['exact'])
 page=search('PDF-10299');check('pdf-passage-page',Path(page[0]['path']).name=='manual.pdf' and page[0]['page']==2 and page[0]['kind']=='pdf')
 ocr=search('SCANNED 77441');check('scanned-pdf-OCR',Path(ocr[0]['path']).name=='scanned.pdf' and ocr[0]['page']==1 and ocr[0]['exact'])
 img=search('A sunny landscape with green hills and a tree',kind='image');check('image-result',img[0]['kind']=='image' and Path(img[0]['path']).name=='untagged-landscape.jpg')
 check('outside-root-not-indexed',all('outside.txt' not in r['path'] for r in search('OUTSIDE-12121')))
 check('score-range-contract',all(-1<=r['score']<=1 for r in exact+img+page+ocr))
 check('set-exclusion',request('set_exclusions',path=str(corpus),exclusions=['excluded'])['ok']);check('excluded-immediately-inactive',all('/excluded/' not in r['path'] for r in search('EXCLUDE-10101')));idle()
 check('invalid-relative-exclusion',not request('set_exclusions',path=str(corpus),exclusions=['../outside'])['ok'])
 (corpus/'identifier.txt').write_text('Replaced maintenance serial NEW-55881 for the ventilation system.')
 (corpus/'payment.txt').rename(corpus/'renamed-payment.md');(corpus/'library.txt').unlink();request('scan');idle()
 rr=search('NEW-55881');check('edit-reconciled',Path(rr[0]['path']).name=='identifier.txt' and rr[0]['exact']);rr=search('My bank card was rejected at checkout');check('rename-delete-reconciled',any(Path(r['path']).name=='renamed-payment.md' for r in rr) and all(Path(r['path']).name not in ['payment.txt','library.txt'] for r in rr))
 # Watcher-triggered edits and deletion need no client scan operation.
 time.sleep(1)
 (corpus/'auto.txt').write_text('WATCH 99173 automatic edit marker')
 wait(lambda s:s['indexed']==10,30)
 time.sleep(1)
 auto=search('WATCH 99173');check('automatic-create-reconciliation',Path(auto[0]['path']).name=='auto.txt' and auto[0]['exact'])
 (corpus/'auto.txt').write_text('WATCH 55628 replacement marker')
 time.sleep(2);idle();auto=search('WATCH 55628');check('automatic-edit-reconciliation',Path(auto[0]['path']).name=='auto.txt' and auto[0]['exact'])
 (corpus/'auto.txt').unlink();time.sleep(2);idle();check('automatic-delete-reconciliation',all(Path(r['path']).name!='auto.txt' for r in search('WATCH 55628')))
 revision=status()['index_revision'];request('scan');idle();check('unchanged-scan-revision-stable',status()['index_revision']==revision)
 (corpus/'fast.txt').write_text('FAST 88332 rapid reconciliation file');time.sleep(2);idle();revision=status()['index_revision'];(corpus/'fast.txt').unlink();request('scan');idle();check('rapid-deletion-advances-index-revision',status()['index_revision']>revision)
 # Pipeline cancellation against actual CPU inference, with status on the same client.
 a=send('search',text='a card transaction was rejected');b=send('cancel',target=a)
 got={}
 while len(got)<2:
  while b'\n' not in buffer:buffer+=conn.recv(65536)
  line,buffer=buffer.split(b'\n',1);r=json.loads(line);results['responses'].append(r);got[r['id']]=r
 check('canceled-request-no-results',not got[a]['ok'] and got[a]['error']=='canceled' and 'results' not in got[a],got)
 (corpus/'aaa-interrupt.txt').write_text(('Interrupted indexing content about mail routes and delivery systems. '*220))
 request('scan');wait(lambda s:s['busy'] and s['current'].endswith('aaa-interrupt.txt'),10);time.sleep(.05)
 check('interrupt-pause',request('pause')['ok']);snapshot_before=status();time.sleep(.3);check('pause-rejects-incomplete-file',status()['segments']==snapshot_before['segments'])
 con=sqlite3.connect(data/'index.sqlite');n=con.execute('select count(*) from segments where path=?',(str(corpus/'aaa-interrupt.txt'),)).fetchone()[0];con.close();check('interrupted-file-no-partial-vectors',n==0,n)
 (corpus/'aaa-interrupt.txt').unlink()
 check('pause',request('pause')['ok']);check('paused-status',status()['paused']);revision=status()['index_revision'];stop();proc=start();check('restart-persists-index-revision',status()['index_revision']==revision);check('restart-config-pause-recovery',status()['paused'] and status()['roots'][0]['exclusions']==['excluded']);request('resume');idle();check('restart-query',search('NEW-55881')[0]['exact'])
 # Terminate service while actual worker is live: EOF must release orphan producer, stale lock and temporary snapshots recover.
 worker_pids={int(v) for x in Path(f'/proc/{proc.pid}/task').glob('*/children') for v in x.read_text().split()};known_workers.update(worker_pids)
 proc.terminate();proc.wait(timeout=10);conn.close();conn=None;buffer=b''
 deadline=time.monotonic()+10
 while time.monotonic()<deadline and any(Path(f'/proc/{pid}').exists() for pid in worker_pids):time.sleep(.1)
 check('terminated-service-worker-closure',not any(Path(f'/proc/{pid}').exists() for pid in worker_pids))
 stale=data/'extract-Ab12Cd';stale.mkdir();(stale/'input.txt').write_text('interrupted private snapshot')
 proc=start();idle();check('restart-removes-interrupted-snapshot',not stale.exists());check('crash-recovery-query',search('NEW-55881')[0]['exact'])
 # Heavy model process must leave after the declared idle interval; inspect native /proc children.
 deadline=time.monotonic()+15;children='unknown'
 while time.monotonic()<deadline:
  children=Path(f'/proc/{proc.pid}/task/{proc.pid}/children').read_text().strip()
  # Producer was created in worker OS thread; inspect all task child lists.
  children=' '.join(x.read_text().strip() for x in Path(f'/proc/{proc.pid}/task').glob('*/children')).strip()
  if not children:break
  time.sleep(.2)
 check('heavy-worker-idle-exit',not children,children)
 check('remove-root',request('remove_root',path=str(corpus))['ok']);check('removed-root-empty',search('NEW-55881')==[]);request('wipe');check('wipe-empty',status()['roots']==[] and status()['segments']==0);stop()
 # Contradictory raw weight: correct-looking name and dimensions never bypass the consumer hash gate.
 bad=runtime/'wrong-model.gguf';bad.write_bytes(b'wrong')
 start_cmd=[str(SERVICE),'--data-dir',str(runtime/'bad-data'),'--socket',sock_path,'--probe',str(P/'build-probe/embedding-probe'),'--text-model',str(bad),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf')]
 log=(out/'bad-model.log').open('wb');badproc=subprocess.Popen(start_cmd,stdout=log,stderr=log);processes.append((badproc,log));time.sleep(.2);conn=socket.socket(socket.AF_UNIX);conn.connect(sock_path);conn.settimeout(10);s=startup_error();check('wrong-model-fails-closed',not s['ready'] and not s['model_ready'] and 'model byte size mismatch' in s['error']);stop()
 # Same expected byte size with contradictory actual bytes must fail the SHA gate.
 bad.write_bytes(b'');bad.open('wb').truncate(309855456)
 log=(out/'bad-model-sha.log').open('wb');badproc=subprocess.Popen(start_cmd,stdout=log,stderr=log);processes.append((badproc,log));time.sleep(.7);conn=socket.socket(socket.AF_UNIX);conn.connect(sock_path);conn.settimeout(10);s=startup_error();check('same-size-wrong-model-SHA-fails-closed',not s['ready'] and 'SHA256 mismatch' in s['error']);stop()
 # Explicit contradictory producer pin fails before loading a model.
 badpin_cmd=start_cmd.copy();badpin_cmd[badpin_cmd.index('--text-model')+1]=str(P/'models/embeddinggemma-2-Q8_0.gguf');badpin_cmd+=['--probe-sha256','0'*64]
 log=(out/'bad-probe-pin.log').open('wb');badproc=subprocess.Popen(badpin_cmd,stdout=log,stderr=log);processes.append((badproc,log));time.sleep(.7);conn=socket.socket(socket.AF_UNIX);conn.connect(sock_path);conn.settimeout(10);s=startup_error();check('explicit-wrong-producer-pin-fails-closed',not s['ready'] and 'unverified producer' in s['error']);stop()
 check('all-indexing-status-samples-responsive',max(results['request_wall_seconds']['status'])<.25,{'max_seconds':max(results['request_wall_seconds']['status'])})
 results['outcome']='pass'
except Exception as error:
 results['outcome']='fail';results['error']=repr(error);raise
finally:
 for proc,log in processes:
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=10)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 if conn:conn.close()
 if all(p.poll() is not None for p,_ in processes) and Path(sock_path).exists():Path(sock_path).unlink();results['stale_socket_cleanup']=True
 results['elapsed_seconds']=time.monotonic()-started
 results['resource_closure']={'known_worker_pids_closed':not any(Path(f'/proc/{pid}').exists() for pid in known_workers),'all_service_processes_exited':all(p.poll() is not None for p,_ in processes),'socket_absent':not Path(sock_path).exists()}
 def sanitize(obj):
  if isinstance(obj,str):return obj.replace(str(corpus),'<corpus>').replace(str(runtime),'<runtime>').replace(str(P),'<project>')
  if isinstance(obj,list):return [sanitize(v) for v in obj]
  if isinstance(obj,dict):return {k:sanitize(v) for k,v in obj.items()}
  return obj
 (out/'results.json').write_text(json.dumps(sanitize(results),indent=2)+'\n')
 print(json.dumps({'outcome':results['outcome'],'checks':len(results['checks']),'output':str(out/'results.json')}))
