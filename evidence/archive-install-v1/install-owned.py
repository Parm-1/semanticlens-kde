import hashlib,json,subprocess
from pathlib import Path
root=Path('local/clean-arch-v1/root.x86_64').resolve()
base=root/'archive-source-v1'; source=base/'kde-index-0.1.0-dev-archivecheck-source'; stage=base/'stage/usr'
files=sorted(x for x in stage.rglob('*') if x.is_file()); assert len(files)==21
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected={str(x.relative_to(stage)):sha(x) for x in files}
assert all(not (root/'usr'/f).exists() and not (root/'usr'/f).is_symlink() for f in expected)
cmd=['bwrap','--bind',str(root),'/','--unshare-user','--uid','0','--gid','0','--unshare-pid','--unshare-ipc','--unshare-uts','--unshare-cgroup','--unshare-net','--die-with-parent','--dev','/dev','--proc','/proc','--tmpfs','/tmp','--clearenv','--setenv','PATH','/usr/bin','--setenv','HOME','/tmp/home','--chdir','/archive-source-v1/kde-index-0.1.0-dev-archivecheck-source','/usr/bin/cmake','--install','build','--prefix','/usr']
with Path('local/archive-install-v1/install.log').open('wb') as log: r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60)
assert r.returncode==0
actual={f:sha(root/'usr'/f) for f in expected}; assert actual==expected
manifest=(source/'build/install_manifest.txt').read_text().splitlines();assert set(manifest)=={'/usr/'+f for f in expected}
receipt={'source_commit':'c394cca550d76b7754ffbbe41bc579b202b0c9ff','command':cmd,'install_exit':r.returncode,'target':'Owned Arch bootstrap root /usr; no host install or network','files':actual,'count':len(actual),'before_conflicts':[],'cmake_manifest_matches':True}
Path('local/archive-install-v1/install-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'installed':len(actual),'manifest_matches':True,'binary_sha256':{k:v for k,v in actual.items() if k.startswith('bin/')}}))
