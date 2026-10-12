#!/usr/bin/env python3
"""Read-only final resource inventory for this review. Does not kill processes."""
from pathlib import Path
import json,os,sys,stat
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v2';known=set();rasters=set();snapshots=[]
def collect(x):
 if isinstance(x,list):
  for v in x:collect(v)
 elif isinstance(x,dict):
  for key,value in x.items():
   if key in ['pid','launcher_pid','listener_pid','replacement_pid','data_owner_pid']:
    try:known.add(int(value))
    except (ValueError,TypeError):pass
   if key in ['children','children_after','launch_pids'] and isinstance(value,list):
    for v in value:
     try:known.add(int(str(v).strip()))
     except ValueError:pass
   if key=='file' and isinstance(value,str) and value.startswith('/tmp/ui-smoke-'):rasters.add(value)
   collect(value)
for name in ['real.json','recovery.json','liveness.json','ownership.json','exact-app.json','smoke.json']:
 collect(json.loads((e/name).read_text()))
for pid in sorted(known):
 try:
  row=Path(f'/proc/{pid}/stat').read_text();f=row[row.rfind(')')+2:].split();status=f[0];ticks=f[19]
 except FileNotFoundError:status='absent';ticks=None
 snapshots.append({'pid':pid,'state':status,'start_ticks':ticks,'closed':status in ['absent','Z','X']})
remaining=[];other=[]
for p in Path('/proc').glob('[0-9]*'):
 try:args=(p/'cmdline').read_bytes().decode().split('\0')
 except (OSError,UnicodeError):continue
 if not args:continue
 exe=Path(args[0])
 if exe.name in ['kde-index-service','embedding-probe','ui-repair','ui-repair-final2','ui-recovery','ui-liveness','ui-smoke','ownership','phase','kde-index']:
  if str(root) in str(exe.resolve()) or any('ui-review-v2' in arg for arg in args[1:]):
   record={'pid':int(p.name),'argv':args[:-1]}
   if int(p.name) in known or any('ui-review-v2' in arg for arg in args):remaining.append(record)
   else:other.append(record)
endpoints=[]
for own in (root/'local').glob('ui-review-v2*'):
 if own.is_dir():
  for p in own.rglob('*'):
   if p.is_symlink():continue
   try:is_socket=stat.S_ISSOCK(p.stat().st_mode)
   except FileNotFoundError:continue
   if is_socket:endpoints.append(str(p.relative_to(root)))
record={'known_pid_checks':snapshots,'remaining_scoped_native_processes':remaining,'other_project_native_processes_left_untouched':other,'remaining_owned_socket_paths':endpoints,'observed_rasters':[{'path':p,'exists':Path(p).exists(),'directory_exists':Path(p).parent.exists()} for p in sorted(rasters)],'real_model_partials':[p.name for p in (root/'models').glob('*.part')],'scope':'ownership is known review PIDs or own ui-review-v2 paths; separate author UI-v3/shared native processes are inventoried separately and never stopped; completed review native helpers/exact GUI and both independent harnesses; absent/exited-zombie PIDs are closed, not active; no child agents; interruption inventory preserved separately'}
(e/'closure.json').write_text(json.dumps(record,indent=2)+'\n')
assert all(x['closed'] for x in snapshots) and not remaining and not endpoints and not any(Path(p).exists() for p in rasters),record
print('closure verified',len(snapshots),'known PIDs; zero scoped native processes/sockets/observed rasters')
