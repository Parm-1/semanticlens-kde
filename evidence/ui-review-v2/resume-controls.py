#!/usr/bin/env python3
"""Resume the declared affected controls against frozen artifacts in an isolated layout.
ROOT is the repository/evidence parent, never a mutable source producer. Completed
successful receipts are preserved; this runner does not imply review acceptance.
"""
from pathlib import Path
import subprocess,json,time,sys,shutil
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v2';layout=root/'local/ui-review-v2-layout'
e.mkdir(parents=True,exist_ok=True);layout.mkdir(parents=True,exist_ok=True)
frozen=root/'evidence/ui-v2/candidate'
for dst,src in [('src',frozen/'src'),('tools',frozen/'tools'),('models',root/'models')]:
 p=layout/dst
 if not p.exists():p.symlink_to(src,target_is_directory=True)
 assert p.resolve()==src.resolve()
(layout/'evidence').mkdir(exist_ok=True)
for name in ['engine-v2','ui-v1']:
 p=layout/'evidence'/name
 if not p.exists():p.symlink_to(root/'evidence'/name,target_is_directory=True)
 assert p.resolve()==(root/'evidence'/name).resolve()
corpus=layout/'local/ui-v2-controls/corpus';corpus.mkdir(parents=True,exist_ok=True)
for name in ['manual.pdf','payment.txt','picture.jpg']:
 p=corpus/name
 if not p.exists():shutil.copy2(root/'evidence/ui-v2/inputs'/name,p)
record=json.loads((e/'run-results.json').read_text()) if (e/'run-results.json').exists() else {'runs':[]}
steps=[('markup',root/'evidence/ui-v2/check-bin/ui-repair-final2',['markup',str(layout)]),('real',frozen/'bin/ui-repair',['real',str(layout)]),('recovery',frozen/'bin/ui-recovery',[str(layout)]),('liveness',frozen/'bin/ui-liveness',[str(layout)]),('smoke',frozen/'bin/ui-smoke',[str(frozen/'bin/kde-index-service'),str(frozen/'bin/embedding-probe'),str(root/'models'),str(layout/'local/smoke-data'),str(layout/'local/smoke.sock'),str(corpus),str(e/'smoke')])]
for name,binary,args in steps:
 if any(x['name']==name and x['exit_code']==0 for x in record['runs']):continue
 start=time.monotonic()
 with (e/(name+'.json')).open('w') as out,(e/(name+'.log')).open('w') as err:
  p=subprocess.run([str(binary),*args],stdout=out,stderr=err,timeout=240)
 record['runs'].append({'name':name,'exit_code':p.returncode,'native_wall_seconds':time.monotonic()-start,'binary':str(binary.relative_to(root))})
 record['scope']='monotonic entire native helper subprocess including functional waits/load/teardown; no billing or performance-distribution inference; preserved surfaces measurement predates interruption'
 (e/'run-results.json').write_text(json.dumps(record,indent=2)+'\n')
 print(name,p.returncode,flush=True)
 if p.returncode:sys.exit(p.returncode)
