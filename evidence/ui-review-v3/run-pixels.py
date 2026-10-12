#!/usr/bin/env python3
from pathlib import Path
import sys,subprocess,json,time
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v3';bin=e/'bin/ui-pixels';py=Path(sys.argv[2]) if len(sys.argv)>2 else Path(sys.executable)
cases=[('smoke-positive',e/'smoke.json',e/'smoke-raster.png',e/'smoke-page.png',0),('render-positive',e/'render.json',e/'render-raster.png',e/'render-page.png',0),('loading-negative',e/'render.json',e/'render-raster.png',e/'render-loading.png',1),('parent-v2-blank-negative',e/'smoke.json',e/'smoke-raster.png',e/'parent-v2-blank.png',1),('wrong-page-source-negative',e/'smoke.json',e/'wrong-page-raster.png',e/'smoke-page.png',1)]
rows=[]
for name,raw,raster,capture,expected in cases:
 for kind,argv in [('candidate',[str(bin),str(raw),str(raster),str(capture)]),('independent',[str(py),str(e/'pixels-independent.py'),str(raw),str(raster),str(capture),str(e/(name+'-independent.json'))])]:
  t=time.monotonic()
  with (e/(name+'-'+kind+'.log')).open('w') as err:
   if kind=='candidate':
    with (e/(name+'-candidate.json')).open('w') as out:p=subprocess.run(argv,stdout=out,stderr=err,timeout=15)
   else:p=subprocess.run(argv,stdout=subprocess.DEVNULL,stderr=err,timeout=15)
  rows.append({'case':name,'consumer':kind,'expected_exit_code':expected,'exit_code':p.returncode,'wall_seconds':time.monotonic()-t})
(e/'pixel-runs.json').write_text(json.dumps({'runs':rows,'scope':'offline pixel consumers of captured geometry/source raster, not native presentation or new runtime metrics'},indent=2)+'\n')
print(json.dumps(rows))
