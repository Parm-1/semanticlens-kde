#!/usr/bin/env python3
"""Actual invalid-byte/U+FFFD sibling and inode/event association controls."""
import ctypes,hashlib,json,os,subprocess,sqlite3,time,argparse
from pathlib import Path
A=argparse.ArgumentParser();A.add_argument('--output',default='local/build-own/local-root-v1/repair-v2/raw-names');a=A.parse_args();P=Path.cwd();O=(P/a.output).resolve();O.mkdir();root=O/'corpus';root.mkdir();invalid=os.fsencode(root)+b'/\xff.txt';valid=root/'\ufffd.txt';fd=os.open(invalid,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'RAW-INVALID-55218 distinct invalid-byte source');os.close(fd);valid.write_text('VALID-REPLACEMENT-99148 genuine valid Unicode sibling');libc=ctypes.CDLL(None,use_errno=True);watch=libc.inotify_init1(os.O_NONBLOCK|os.O_CLOEXEC);wd=libc.inotify_add_watch(watch,invalid,0x21)
def events():
 result=b''
 while True:
  try:result+=os.read(watch,65536)
  except BlockingIOError:break
 return result
with open(invalid,'rb') as f:positive_before=f.read()
armed=events();probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';log=(O/'stderr.log').open('w');p=subprocess.Popen([str(P/'local/build-own/local-root-v1/repair-v2/build/local-root-controls'),str(O/'data'),str(probe),str(P/'models/embeddinggemma-2-Q8_0.gguf'),str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),hashlib.sha256(probe.read_bytes()).hexdigest()],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True);raw=[];workers=set();begin=time.monotonic()
def read():
 r=json.loads(p.stdout.readline());raw.append(r);return r
def req(op,**kw):p.stdin.write(json.dumps(dict(op=op,**kw))+'\n');p.stdin.flush();return read()
try:
 initial=read();workers.update(int(v) for f in Path(f'/proc/{p.pid}/task').glob('*/children') for v in f.read_text().split());req('add_root',path=str(root));state=req('wait');valid_query=req('search',text='VALID-REPLACEMENT-99148');invalid_query=req('search',text='RAW-INVALID-55218');pressure=req('pressure_scan');retained=req('search',text='VALID-REPLACEMENT-99148');req('scan');recovered=req('wait');p.stdin.write('{"op":"exit"}\n');p.stdin.flush();p.stdin.close();p.wait(timeout=20);interval=events()
 with open(invalid,'rb') as f:positive_after=f.read()
 after=events();db=sqlite3.connect(O/'data/index.sqlite');rows=db.execute('select path,text from pages').fetchall();db.close();checks={'different-native-inodes':os.stat(invalid).st_ino!=valid.stat().st_ino,'explicit-invalid-body-positive-before-and-after':bool(armed) and bool(after) and positive_before==positive_after,'non-roundtrip-raw-name-observable-skip':state['status']['scan_skipped_names']==1 and state['status']['total']==1 and state['status']['scan_entries']==2,'only-valid-sibling-body-associated':len(rows)==1 and rows[0]==(str(valid),valid.read_text()) and valid_query['reply']['results'][0]['exact'],'invalid-body-not-a-literal-result':all(not r['exact'] for r in invalid_query['reply']['results']),'invalid-inode-no-open-or-access-during-engine-interval':interval==b'','actual-fd-exhaustion-preserves-old-source-and-recovers':bool(pressure['reply'].get('error')) and retained['reply']['results'][0]['exact'] and recovered['status']['scan_error']==''};outcome='pass' if all(checks.values()) else 'fail'
finally:
 if p.poll() is None:p.terminate()
 p.wait(timeout=20);log.close();os.close(watch)
 result={'outcome':locals().get('outcome','fail'),'checks':locals().get('checks',{}),'raw':raw,'raw_name_hex':'ff2e747874','invalid_inode':os.stat(invalid).st_ino,'valid_inode':valid.stat().st_ino,'inotify':{'watch_descriptor':wd,'positive_before_hex':armed.hex(),'engine_interval_hex':locals().get('interval',b'').hex(),'positive_after_hex':locals().get('after',b'').hex()},'serialized_pages':locals().get('rows',[]),'wall_seconds':time.monotonic()-begin,'closure':{'process_closed':p.poll() is not None,'workers_closed':not any(Path(f'/proc/{pid}').exists() for pid in workers),'known_worker_pids':sorted(workers)}};(O/'results.json').write_text(json.dumps(result,indent=2)+'\n');(O/'responses.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in raw));print(json.dumps({'outcome':result['outcome'],'checks':len(result['checks']),'closure':result['closure']}))
