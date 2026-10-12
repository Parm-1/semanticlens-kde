import subprocess,json,time,os,signal,stat
from pathlib import Path
out=Path('/tmp/startup')
for kind in ('relative-negative','absolute-positive'):
 run=out/kind;run.mkdir()
 for name in ('native-runtime','native-home','native-config','native-data','native-cache'):
  (run/name).mkdir(mode=0o700)
 env={**os.environ,'HOME':str(run/'native-home'),'XDG_RUNTIME_DIR':str(run/'native-runtime'),'XDG_CONFIG_HOME':str(run/'native-config'),'XDG_DATA_HOME':str(run/'native-data'),'XDG_CACHE_HOME':str(run/'native-cache')}
 sock='native-runtime/smoke.sock' if kind=='relative-negative' else str(run/'native-runtime/smoke.sock')
 cmd=['/tmp/startup/build/startup-observer','/tmp/stage/bin/kde-index-service','/tmp/stage/bin/embedding-probe','/models',sock,str(run/'native-data')]
 receipt={'command':cmd,'cwd':str(run),'env':env,'initial_modes':{name:oct(stat.S_IMODE((run/name).stat().st_mode)) for name in ('native-runtime','native-data')},'process_samples':[]}
 def inventory():
  rows=[]
  for q in Path('/proc').iterdir():
   if not q.name.isdigit():continue
   try:
    cmd=(q/'cmdline').read_bytes().replace(b'\0',b' ').decode();status=(q/'stat').read_text();state=status[status.rfind(')')+2:].split()[0]
    if any(s in cmd for s in ('startup-observer','kde-index-service','embedding-probe')):rows.append({'pid':int(q.name),'cmdline':cmd,'state':state})
   except OSError:pass
  return rows
 start=time.monotonic()
 with (run/'stdout.jsonl').open('wb') as stdout,(run/'stderr.log').open('wb') as stderr:
  proc=subprocess.Popen(cmd,cwd=run,env=env,stdout=stdout,stderr=stderr)
  while proc.poll() is None and time.monotonic()-start<18:
   receipt['process_samples'].append({'ms':round((time.monotonic()-start)*1000),'processes':inventory()});time.sleep(.1)
  if proc.poll() is None:proc.terminate()
  receipt['observer_exit']=proc.wait(timeout=3)
 receipt['elapsed_ms']=round((time.monotonic()-start)*1000)
 # Capture closure before namespace teardown; failed detached launches may leave zombies.
 for _ in range(30):
  remaining=inventory()
  if not any(row['state'] not in ('Z','X') for row in remaining):break
  time.sleep(.1)
 receipt['remaining_scoped_processes']=remaining
 receipt['socket_exists']=Path(sock).exists() if sock.startswith('/') else (run/sock).exists()
 receipt['data_files']=[p.name for p in (run/'native-data').iterdir()]
 # namespace close is ultimate closure for any unexpected live child; document intervention.
 receipt['forced_live_cleanup']=[]
 for row in remaining:
  if row['state'] not in ('Z','X'):
   receipt['forced_live_cleanup'].append(row);os.kill(row['pid'],signal.SIGTERM)
 (run/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps({'kind':kind,'exit':receipt['observer_exit'],'ms':receipt['elapsed_ms'],'remaining':remaining,'data_files':receipt['data_files']}),flush=True)
