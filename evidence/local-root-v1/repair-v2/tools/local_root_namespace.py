#!/usr/bin/env python3
"""Owned, private-namespace kernel producer controls; no host mounts."""
import argparse,ctypes,hashlib,json,os,socket,subprocess,time
from pathlib import Path
A=argparse.ArgumentParser();A.add_argument('--inside',action='store_true');A.add_argument('--output',required=True);A.add_argument('--candidate',default='local/build-own/local-root-v1/repair-v2/build/local-root-service');A.add_argument('--provider',default='local/build-own/local-root-v1/repair-v2/build/local-root-provider');a=A.parse_args();P=Path.cwd();O=Path(a.output).resolve()
if not a.inside:
 if O.exists():raise SystemExit('preserve previous namespace receipt')
 O.mkdir(parents=True);cmd=['bwrap','--unshare-all','--uid','0','--gid','0','--cap-add','CAP_SYS_ADMIN','--new-session','--die-with-parent','--ro-bind','/','/','--bind',str(O),str(O),'--dev','/dev','--dev-bind','/dev/fuse','/dev/fuse','--proc','/proc','--tmpfs','/tmp','python3',str(Path(__file__).resolve()),'--inside','--output',str(O),'--candidate',a.candidate,'--provider',a.provider]
 with (O/'namespace.log').open('wb') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 print(json.dumps({'namespace_exit':r.returncode,'output':str(O/'results.json')}));raise SystemExit(r.returncode)
libc=ctypes.CDLL(None,use_errno=True);checks=[];responses=[];services=[];workers=set();mounts=[];daemon=None;begin=time.monotonic();callback=O/'callbacks.log';provider=O/'provider';provider.mkdir();corpus=O/'corpus';corpus.mkdir();nested=corpus/'local';nested.mkdir();foreign=corpus/'provider';foreign.mkdir();tmp=Path('/tmp/local-root-owned-source');tmp.mkdir();(tmp/'local.txt').write_text('OWNED-LOCAL-MOUNT-63128 actual approved nested file body')
(corpus/'normal.txt').write_text('OWNED-NORMAL-31028 ordinary local source body');(provider/'provider.txt').write_text('LEGACY-ROOT-12948 previously accepted ordinary content');(foreign/'provider.txt').write_text('LEGACY-NESTED-92814 previously accepted nested ordinary content')
probe=P/'local/installed-host-v3/stage/usr/bin/embedding-probe';probe_sha=hashlib.sha256(probe.read_bytes()).hexdigest();original=P/'local/installed-host-v3/stage/usr/bin/kde-index-service';candidate=(P/a.candidate).resolve();data=O/'data';rid=0
class Service:
 def __init__(self,binary):
  global rid
  self.socket=str(O/f'service-{len(services)}.socket');self.log=(O/f'service-{len(services)}.log').open('wb');self.p=subprocess.Popen([str(binary),'--data-dir',str(data),'--socket',self.socket,'--probe',str(probe),'--probe-sha256',probe_sha,'--text-model',str(P/'models/embeddinggemma-2-Q8_0.gguf'),'--image-model',str(P/'models/mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','2'],stdout=self.log,stderr=self.log);services.append(self);self.buf=b''
  for i in range(300):
   try:self.c=socket.socket(socket.AF_UNIX);self.c.connect(self.socket);self.c.settimeout(60);break
   except OSError:
    if self.p.poll() is not None:raise RuntimeError('service exited')
    time.sleep(.02)
  self.wait(lambda s:s['ready'])
 def req(self,op,**kw):
  global rid
  rid+=1;self.c.sendall(json.dumps(dict(op=op,id=str(rid),**kw)).encode()+b'\n')
  while b'\n' not in self.buf:self.buf+=self.c.recv(65536)
  line,self.buf=self.buf.split(b'\n',1);r=json.loads(line);assert r['id']==str(rid);responses.append({'op':op,'reply':r});return r
 def wait(self,pred):
  for i in range(1500):
   s=self.req('status')['status'];workers.update(int(v) for f in Path(f'/proc/{self.p.pid}/task').glob('*/children') for v in f.read_text().split())
   if pred(s):return s
   time.sleep(.02)
  raise RuntimeError('status timeout '+str(s))
 def close(self):self.req('shutdown');self.c.close();self.p.wait(timeout=20);self.log.close()
def check(name,value,detail=None):
 checks.append({'name':name,'passed':bool(value),'detail':detail})
 if not value:raise AssertionError(name)
def bind(source,target):
 if libc.mount(os.fsencode(source),os.fsencode(target),None,4096,None)!=0:raise OSError(ctypes.get_errno(),'private bind failed')
 mounts.append(target)
def mounted(target):
 for line in Path('/proc/self/mountinfo').read_text().splitlines():
  parts=line.split(' ');point=parts[4].replace('\\040',' ').replace('\\134','\\')
  if point==str(target):
   split=parts.index('-');return {'id':int(parts[0]),'parent':int(parts[1]),'device':parts[2],'path':point,'type':parts[split+1],'raw':line}
 return None
def callbacks():return callback.read_text() if callback.exists() else ''
try:
 bind(tmp,nested);local_mount=mounted(nested);check('actual-approved-tmpfs-nested-mount',local_mount and local_mount['type']=='tmpfs',local_mount);check('actual-local-bind-inode',os.stat(tmp/'local.txt').st_ino==os.stat(nested/'local.txt').st_ino)
 # Actual old accepted engine creates vectors at paths later replaced by provider mounts.
 old=Service(original);check('legacy-ordinary-root-admitted',old.req('add_root',path=str(provider))['ok']);old.wait(lambda s:not s['busy']);check('legacy-corpus-admitted',old.req('add_root',path=str(corpus))['ok']);s=old.wait(lambda s:not s['busy']);check('actual-legacy-four-files-indexed',s['indexed']==4,s);check('actual-legacy-root-literal',old.req('search',text='LEGACY-ROOT-12948')['results'][0]['exact']);check('actual-legacy-nested-literal',old.req('search',text='LEGACY-NESTED-92814')['results'][0]['exact']);old.close()
 env=os.environ.copy();env['LOCAL_ROOT_PROVIDER_LOG']=str(callback);daemon_log=(O/'provider.log').open('wb');daemon=subprocess.Popen([str((P/a.provider).resolve()),'-f','-o','attr_timeout=0,entry_timeout=0',str(provider)],env=env,stdout=daemon_log,stderr=daemon_log)
 for i in range(300):
  provider_mount=mounted(provider)
  if provider_mount:break
  if daemon.poll() is not None:raise RuntimeError('provider exited '+str(daemon.returncode))
  time.sleep(.01)
 check('actual-fuse-mount-type',provider_mount and provider_mount['type'].startswith('fuse'),provider_mount);mounts.append(provider);bind(provider,foreign);nested_mount=mounted(foreign);check('actual-unsupported-nested-bind-type',nested_mount and nested_mount['type'].startswith('fuse'),nested_mount)
 # Independent explicit source body read arms the exact mounted producer.
 positive_before=(foreign/'provider.txt').read_text();check('provider-source-positive-before',positive_before.startswith('OWNED-PROVIDER-73921') and 'read /provider.txt' in callbacks());raw_metadata=subprocess.check_output([str(P/'local/build-own/local-root-v1/build/local-root-metadata'),str(foreign/'provider.txt')],text=True);(O/'provider-metadata.jsonl').write_text(raw_metadata);check('ordinary-provider-fields-real-kernel',json.loads(raw_metadata)['rc']==0 and json.loads(raw_metadata)['mode']&0o170000==0o100000)
 endpoint_before=callbacks();(O/'callback-positive-before.log').write_text(endpoint_before)
 new=Service(candidate);s=new.req('status')['status'];check('restart-purges-actual-legacy-root-and-nested-provider-content',s['indexed']==2,s)
 for text in ['LEGACY-ROOT-12948','LEGACY-NESTED-92814']:
  rr=new.req('search',text=text)['results'];check('restart-old-provider-path-not-searchable-'+text,all('/provider' not in r['path'].replace(str(O),'') for r in rr),rr)
 rejected=new.req('add_root',path=str(provider));check('provider-add-rejected-before-read',not rejected['ok'] and 'unsupported storage provider' in rejected['error'],rejected)
 check('supported-resume-with-skipped-provider',new.req('resume')['ok']);s=new.wait(lambda s:not s['busy']);check('nested-provider-warning-visible',s['scan_skipped_providers']==2 and any(x['path']==str(foreign) for x in s['scan_warnings']),s)
 rr=new.req('search',text='OWNED-LOCAL-MOUNT-63128')['results'];check('actual-approved-nested-body-and-literal-positive',rr[0]['path']==str(nested/'local.txt') and rr[0]['exact'] and 'actual approved nested file body' in rr[0]['text'],rr[0]);rr=new.req('search',text='OWNED-NORMAL-31028')['results'];check('ordinary-positive-retained',rr[0]['exact'] and rr[0]['path']==str(corpus/'normal.txt'))
 alias=O/'alias';alias.symlink_to(corpus);check('owned-ancestor-symlink-source-positive',(alias/'local/local.txt').read_text().startswith('OWNED-LOCAL-MOUNT-63128'))
 prefix=O/'nul-prefix';prefix.mkdir();(prefix/'source.txt').write_text('OWNED-NUL-PREFIX-39217 actual prefix directory body');check('real-prefix-body-positive',(prefix/'source.txt').read_text().startswith('OWNED-NUL-PREFIX-39217'));before_roots=new.req('status')['status']['roots']
 nul=new.req('add_root',path=str(prefix)+'\x00suffix');check('embedded-NUL-rejected-before-prefix-resolution',not nul['ok'] and 'NUL' in nul['error'],nul);check('NUL-rejection-no-root-mutation',new.req('status')['status']['roots']==before_roots)
 check('exact-unselected-prefix-root-accepted',new.req('add_root',path=str(prefix))['ok']);new.wait(lambda s:not s['busy']);check('exact-prefix-genuine-body-retrieved',new.req('search',text='OWNED-NUL-PREFIX-39217')['results'][0]['exact'])
 spelling=new.req('add_root',path=str(corpus)+'/../corpus');check('dot-component-root-rejected-before-cleanup',not spelling['ok'] and 'dot components' in spelling['error'],spelling)
 link=new.req('add_root',path=str(alias/'local'));check('ancestor-symlink-root-rejected',not link['ok'] and 'local path resolution rejected' in link['error'],link)
 check('remove-rejected-old-root',new.req('remove_root',path=str(provider))['ok']);check('exclude-supported-local-nested',new.req('set_exclusions',path=str(corpus),exclusions=['local'])['ok']);new.wait(lambda s:not s['busy']);check('excluded-local-content-inactive',all(r['path']!=str(nested/'local.txt') for r in new.req('search',text='OWNED-LOCAL-MOUNT-63128')['results']))
 check('explicit-wipe-removes-legacy-state',new.req('wipe')['removed']);check('wipe-empty',new.req('status')['status']['roots']==[]);check('new-supported-corpus-admitted',new.req('add_root',path=str(corpus))['ok']);s=new.wait(lambda s:not s['busy']);check('fresh-scan-supported-nested-positive-provider-skipped',s['indexed']==2 and s['scan_skipped_providers']==1,s);new.close()
 endpoint_after=callbacks();(O/'callback-engine-interval.log').write_text(endpoint_after[len(endpoint_before):]);check('no-provider-metadata-or-body-callback-during-all-engine-phases',endpoint_after==endpoint_before)
 positive_after=(foreign/'provider.txt').read_text();check('provider-source-positive-after',positive_after==positive_before and len(callbacks())>len(endpoint_after) and 'read /provider.txt' in callbacks()[len(endpoint_after):]);(O/'callback-positive-after.log').write_text(callbacks()[len(endpoint_after):]);outcome='pass'
except Exception as e:outcome='fail';error=repr(e);raise
finally:
 for service in services:
  if service.p.poll() is None:service.p.terminate()
  try:service.p.wait(timeout=20)
  except subprocess.TimeoutExpired:service.p.kill();service.p.wait()
  service.log.close()
 unmount=[]
 for target in reversed(mounts):
  rc=libc.umount2(os.fsencode(target),0);unmount.append({'path':str(target),'rc':rc,'errno':ctypes.get_errno() if rc else 0})
 if daemon:
  try:daemon.wait(timeout=5)
  except subprocess.TimeoutExpired:daemon.terminate();daemon.wait(timeout=5)
 result={'outcome':locals().get('outcome','fail'),'error':locals().get('error'),'checks':checks,'responses':responses,'probe_sha256':probe_sha,'wall_seconds':time.monotonic()-begin,'resource_closure':{'services_closed':all(s.p.poll() is not None for s in services),'workers_closed':not any(Path(f'/proc/{pid}').exists() for pid in workers),'provider_closed':daemon is None or daemon.poll() is not None,'unmounts':unmount,'sockets_absent':all(not Path(s.socket).exists() for s in services)},'usage':'package token and billing metrics unavailable; elapsed is whole inside-namespace runner interval'}
 (O/'results.json').write_text(json.dumps(result,indent=2)+'\n')
