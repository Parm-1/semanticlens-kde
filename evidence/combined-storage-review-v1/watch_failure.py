#!/usr/bin/env python3
"""Independent actual-service controls; all state is owned and private."""
import hashlib,json,os,re,socket,sqlite3,subprocess,time
from pathlib import Path
P=Path.cwd(); O=P/'local/build-own/combined-storage-review-v1/watch-failure'; O.mkdir()
B=P/'local/build-integrated-v6/bin/kde-index-service'; probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe'
parent=O/'parent';parent.mkdir();root=parent/'corpus';root.mkdir();source=root/'source.txt';body='REVIEW-RETAIN-66129 genuine old literal source';source.write_text(body)
data=O/'data';sock=O/'service.sock';log=(O/'service.log').open('wb');p=subprocess.Popen([str(B),'--data-dir',str(data),'--socket',str(sock),'--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','2'],stdout=log,stderr=log)
raw=[];rid=0;buf=b'';c=None;workers=set();begin=time.monotonic_ns()
def req(op,**kw):
 global rid,buf
 rid+=1;c.sendall(json.dumps(dict(op=op,id=str(rid),**kw)).encode()+b'\n')
 while b'\n' not in buf:
  chunk=c.recv(65536)
  if not chunk:raise RuntimeError('closed service')
  buf+=chunk
 line,buf=buf.split(b'\n',1);reply=json.loads(line);assert reply['id']==str(rid);raw.append(dict(clock_ns=time.monotonic_ns(),op=op,args=kw,reply=reply));return reply
def wait(pred):
 for _ in range(1500):
  s=req('status')['status'];
  for f in Path(f'/proc/{p.pid}/task').glob('*/children'):workers.update(map(int,f.read_text().split()))
  if pred(s):return s
  time.sleep(.02)
 raise RuntimeError('status timeout')
def pages():
 with sqlite3.connect(data/'index.sqlite') as db:return db.execute('select path,text from pages order by path').fetchall()
def watches():
 return {f.name:f.read_text() for f in Path(f'/proc/{p.pid}/fdinfo').iterdir() if 'inotify wd:' in f.read_text()}
try:
 for _ in range(500):
  try:c=socket.socket(socket.AF_UNIX);c.settimeout(90);c.connect(str(sock));break
  except OSError:c.close();c=None;time.sleep(.02)
 assert c is not None;initial=wait(lambda s:s['ready']);source.unlink();req('add_root',path=str(root));before_watch=watches();root.rmdir();failed=wait(lambda s:bool(s.get('watch_error')));idle_start=req('status')['status'];time.sleep(11);idle_end=req('status')['status'];req('remove_root',path=str(root));time.sleep(.7);removed=req('status')['status'];result=dict(before_watch=before_watch,failed=failed,idle_start=idle_start,idle_end=idle_end,removed=removed,observations={'all_add_paths_failed_before_watch':not before_watch and bool(failed['watch_error']),'all_failed_batch_no_reconciliation':idle_start.get('scan_error','')==idle_end.get('scan_error','')=='' and idle_start['index_revision']==idle_end['index_revision']==0,'zero_roots_notice_clears':removed['roots']==[] and removed['watch_error']==''});req('shutdown');c.close();c=None;p.wait(timeout=30)
finally:
 if c:c.close()
 if p.poll() is None:p.terminate();p.wait(timeout=30)
 log.close();result=locals().get('result',{});result.update(raw=raw,elapsed_ns=time.monotonic_ns()-begin,service_sha256=hashlib.sha256(B.read_bytes()).hexdigest(),closure={'service_exit':p.returncode,'socket_absent':not sock.exists(),'workers_absent':not any(Path(f'/proc/{pid}').exists() for pid in workers),'known_workers':sorted(workers)});(O/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result.get(k) for k in ['observations','closure']}))
