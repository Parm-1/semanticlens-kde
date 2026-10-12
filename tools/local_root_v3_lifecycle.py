#!/usr/bin/env python3
"""Independent actual-service controls; all state is owned and private."""
import hashlib,json,os,re,socket,sqlite3,subprocess,time
from pathlib import Path
P=Path.cwd(); O=P/'local/build-own/local-root-v1/repair-v3/lifecycle-replay'; O.mkdir()
B=P/'local/build-own/local-root-v1/repair-v3/build/local-root-service'; probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe'
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
 result={}
 for f in Path(f'/proc/{p.pid}/fdinfo').iterdir():
  try:v=f.read_text()
  except FileNotFoundError:continue
  if 'inotify wd:' in v:result[f.name]=v
 return result
try:
 for _ in range(500):
  try:c=socket.socket(socket.AF_UNIX);c.settimeout(90);c.connect(str(sock));break
  except OSError:c.close();c=None;time.sleep(.02)
 assert c is not None;initial=wait(lambda s:s['ready']);req('add_root',path=str(root));before_watch=watches();auto=root/'auto.txt';auto_body='REVIEW-WATCH-93182 created before actual watch';creation=[time.monotonic_ns()];auto.write_text(auto_body);creation.append(time.monotonic_ns());settled=wait(lambda s:not s['busy'] and s['indexed']==2);after_watch=watches();auto_query=req('search',text='REVIEW-WATCH-93182');base_query=req('search',text='REVIEW-RETAIN-66129');positive_pages=pages()
 # Actual incomplete enumeration must preserve old serialized content.
 req('pause');parent.chmod(0);failed_scan=req('scan');after_failed_scan=pages();failed_exclusions=req('set_exclusions',path=str(root),exclusions=['never-present']);after_exclusions=pages();parent.chmod(0o700);req('resume');recovered=wait(lambda s:not s['busy']);recovery_query=req('search',text='REVIEW-RETAIN-66129')
 # Definite exclusion still removes its own content while the sibling remains.
 req('set_exclusions',path=str(root),exclusions=['auto.txt']);wait(lambda s:not s['busy']);excluded_pages=pages();excluded_query=req('search',text='REVIEW-WATCH-93182')
 inode=root.stat().st_ino
 def watched(raw):return any(int(x,16)==inode for v in raw.values() for x in re.findall(r'inotify wd:\d+ ino:([0-9a-fA-F]+)',v))
 result=dict(initial=initial,creation_clock_ns=creation,before_watch=before_watch,after_watch=after_watch,positive_pages=positive_pages,after_failed_scan=after_failed_scan,after_exclusions=after_exclusions,excluded_pages=excluded_pages,observations={'root_unwatched_at_creation':not watched(before_watch),'root_watched_after':watched(after_watch),'automatic_exact_path_body':any(r['path']==str(auto) and r['text']==auto_body and r['exact'] for r in auto_query['results']),'baseline_source_exact_path_body':any(r['path']==str(source) and r['text']==body and r['exact'] for r in base_query['results']),'EACCES_scan_preserves_pages':after_failed_scan==positive_pages and 'Permission denied' in failed_scan.get('error',''),'EACCES_unrelated_exclusion_preserves_pages':after_exclusions==positive_pages,'permission_recovery_genuine_body':any(r['path']==str(source) and r['text']==body and r['exact'] for r in recovery_query['results']),'definite_exclusion_removes_only_matching_page':excluded_pages==[(str(source),body)] and all(r['path']!=str(auto) for r in excluded_query['results'])})
 req('shutdown');c.close();c=None;p.wait(timeout=30)
finally:
 parent.chmod(0o700)
 if c:c.close()
 if p.poll() is None:p.terminate();p.wait(timeout=30)
 log.close();result=locals().get('result',{});result.update(raw=raw,elapsed_ns=time.monotonic_ns()-begin,service_sha256=hashlib.sha256(B.read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),closure={'service_exit':p.returncode,'socket_absent':not sock.exists(),'workers_absent':not any(Path(f'/proc/{pid}').exists() for pid in workers),'known_workers':sorted(workers),'permissions_restored':parent.stat().st_mode&0o777==0o700});(O/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result.get(k) for k in ['observations','closure']}))
