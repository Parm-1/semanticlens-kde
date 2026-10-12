import hashlib,json,subprocess
from pathlib import Path
P=Path.cwd();root=P/'local/clean-arch-v1/root.x86_64';root=root.resolve();base=root/'public-install-v2';base.mkdir();stage=root/'public-clean-v2/stage/usr'
expected={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest() for p in stage.rglob('*') if p.is_file()}
record=json.loads((P/'evidence/public-clean-build-v2/stage-manifest.json').read_text());assert len(expected)==21 and expected=={p['path'].removeprefix('usr/'):p['sha256'] for p in record['files']}
assert all(not (root/'usr'/f).exists() and not (root/'usr'/f).is_symlink() for f in expected)
old_manifest=root/'public-clean-v2/source/build/install_manifest.txt';old_hash=hashlib.sha256(old_manifest.read_bytes()).hexdigest()
manifest=base/'install_manifest.txt';manifest.touch()
cmd=['bwrap','--die-with-parent','--unshare-all','--ro-bind',str(root),'/','--bind',str(root/'usr'),'/usr','--bind',str(base),'/public-install-v2','--bind',str(manifest),'/public-clean-v2/source/build/install_manifest.txt','--proc','/proc','--dev','/dev','--tmpfs','/tmp','--clearenv','--setenv','PATH','/usr/bin','--setenv','HOME','/tmp/home','--chdir','/public-clean-v2/source/kde-index-source','/usr/bin/cmake','--install','../build','--prefix','/usr']
with (base/'install.log').open('wb') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60)
assert r.returncode==0
actual={f:hashlib.sha256((root/'usr'/f).read_bytes()).hexdigest() for f in expected};assert actual==expected
assert set(manifest.read_text().splitlines())=={'/usr/'+f for f in expected}
assert hashlib.sha256(old_manifest.read_bytes()).hexdigest()==old_hash
receipt=dict(source_commit='5beacaebb65427478e7e2dd820c8cbea64cf13ea',command=cmd,install_exit=r.returncode,files=actual,count=len(actual),before_conflicts=[],cmake_manifest_matches=True,frozen_build_manifest_unchanged=True,scope='Actual /usr in owned bootstrap only; bootstrap system dependencies preserved; host untouched; source/build readonly with install manifest redirected to new owned receipt')
(base/'install-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'installed':len(actual),'build_manifest_preserved':True}))
