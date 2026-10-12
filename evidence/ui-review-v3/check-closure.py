#!/usr/bin/env python3
"""Read-only review-owned process/socket/raster closure. Does not stop parent jobs."""
from pathlib import Path
import json,sys,stat
root=Path(sys.argv[1]).resolve();e=root/'evidence/ui-review-v3';temp=[]
for name in ['smoke','render']:
 data=json.loads((e/(name+'.json')).read_text())
 for row in data.get('checks',[]):
  for key in ['file','raster']:
   if isinstance(row.get(key),str) and row[key].startswith('/tmp/'):temp.append(row[key])
processes=[];other=[]
for p in Path('/proc').glob('[0-9]*'):
 try:args=(p/'cmdline').read_bytes().decode().split('\0')
 except (OSError,UnicodeError):continue
 if not args:continue
 if Path(args[0]).name in ['kde-index-service','embedding-probe','ui-render','ui-smoke','ui-pixels','kde-index']:
  row={'pid':int(p.name),'argv':args[:-1]}
  if any('ui-review-v3' in arg for arg in args):processes.append(row)
  elif str(root) in str(Path(args[0]).resolve()):other.append(row)
sockets=[]
for own in list((root/'local').glob('ui-review-v3*'))+[e]:
 if own.is_dir():
  for p in own.rglob('*'):
   if p.is_symlink():continue
   try:is_socket=stat.S_ISSOCK(p.stat().st_mode)
   except FileNotFoundError:continue
   if is_socket:sockets.append(str(p.relative_to(root)))
record={'remaining_review_native_processes':processes,'other_project_native_processes_left_untouched':other,'remaining_review_sockets':sockets,'recorded_temp_rasters':[{'path':p,'exists':Path(p).exists(),'directory_exists':Path(p).parent.exists()} for p in sorted(set(temp))],'scope':'review-owned prefixes/native helpers; retained evidence raster copies are intentional; separate parent/author jobs untouched; no children delegated'}
(e/'closure.json').write_text(json.dumps(record,indent=2)+'\n')
assert not processes and not sockets and not any(Path(p).exists() for p in temp),record
print('review closure verified')
