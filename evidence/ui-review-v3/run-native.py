#!/usr/bin/env python3
"""Narrow frozen UI-v3 render/accessibility controls; no candidate mutation."""
from pathlib import Path
import sys,subprocess,time,json,shutil
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v3';own=root/'local/ui-review-v3-smoke';c=root/'evidence/ui-v3/candidate';corpus=own/'corpus';corpus.mkdir(parents=True,exist_ok=True)
for src,name in [(e/'inputs/manual.pdf','manual.pdf'),(e/'inputs/payment.txt','payment.txt'),(e/'inputs/picture.jpg','picture.jpg')]:
 if not (corpus/name).exists():shutil.copy2(src,corpus/name)
commands=[('smoke',[str(e/'bin/ui-smoke'),str(root/'evidence/engine-v2/candidate/bin/kde-index-service'),str(root/'evidence/ui-v1/candidate/bin/embedding-probe'),str(root/'models'),str(own/'data'),str(own/'service.sock'),str(corpus),str(e/'smoke')],100),('render',[str(e/'bin/ui-render'),str(e/'source/Main.qml'),str(e/'inputs/manual.pdf'),str(e/'render')],45)]
runs=[]
for name,argv,bound in commands:
 start=time.monotonic()
 with (e/(name+'.json')).open('w') as out,(e/(name+'.log')).open('w') as err:p=subprocess.run(argv,stdout=out,stderr=err,timeout=bound)
 runs.append({'name':name,'exit_code':p.returncode,'native_wall_seconds':time.monotonic()-start,'argv':argv,'outer_bound_seconds':bound})
 (e/'native-runs.json').write_text(json.dumps({'runs':runs,'scope':'whole native subprocess, including functional waits/load/teardown; diagnostic wall scope, no presentation/latency/billing claim'},indent=2)+'\n')
 print(name,p.returncode,flush=True)
 if p.returncode:sys.exit(p.returncode)
