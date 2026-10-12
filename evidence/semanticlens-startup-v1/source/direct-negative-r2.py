import subprocess,json,hashlib,time,os
from pathlib import Path
run=Path('/tmp/startup/direct-relative-negative-r2');run.mkdir();(run/'native-runtime').mkdir(mode=0o700);(run/'native-data').mkdir(mode=0o700)
probe=Path('/tmp/stage/bin/embedding-probe')
cmd=['/tmp/stage/bin/kde-index-service','--data-dir',str(run/'native-data'),'--socket','native-runtime/smoke.sock','--probe',str(probe),'--probe-sha256',hashlib.sha256(probe.read_bytes()).hexdigest(),'--text-model','/models/embeddinggemma-2-Q8_0.gguf','--image-model','/models/mmproj-embeddinggemma-2-Q8_0.gguf']
t=time.monotonic()
with (run/'stdout.log').open('wb') as o,(run/'stderr.log').open('wb') as e:p=subprocess.run(cmd,cwd=run,stdout=o,stderr=e,timeout=5,env={**os.environ,"QT_FORCE_STDERR_LOGGING":"1"})
(run/'receipt.json').write_text(json.dumps({'command':cmd,'cwd':str(run),'exit':p.returncode,'elapsed_ms':round((time.monotonic()-t)*1000),'data_files':[p.name for p in (run/'native-data').iterdir()],'socket_exists':(run/'native-runtime/smoke.sock').exists()},indent=2)+'\n');print(p.returncode)
