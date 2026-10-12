import hashlib,json
from pathlib import Path
root=Path('local/clean-arch-v1/root.x86_64').resolve(); assert root.name=='root.x86_64' and root.parent.name=='clean-arch-v1'
r=json.loads(Path('local/archive-install-v1/install-receipt.json').read_text())
files=r['files']; assert len(files)==21
for f,h in files.items():
 p=root/'usr'/f
 assert p.is_relative_to(root/'usr') and not p.is_symlink() and hashlib.sha256(p.read_bytes()).hexdigest()==h
for f in files:(root/'usr'/f).unlink()
assert all(not (root/'usr'/f).exists() for f in files)
Path('local/archive-install-v1/remove-receipt.json').write_text(json.dumps({'removed_paths':['/usr/'+f for f in files],'count':len(files),'hashes_verified_before_removal':True,'all_absent_after':True,'scope':'Exact initially absent owned-userspace files only; no host files or dependencies removed; no CMake uninstall target claimed'},indent=2)+'\n')
print('Removed 21 verified owned-userspace application files; all are absent.')
