import hashlib,json,subprocess
from pathlib import Path
P=Path.cwd();root=(P/'local/clean-arch-v1/root.x86_64').resolve();base=root/'public-stage-v3';base.mkdir();manifest=base/'install_manifest.txt';manifest.touch()
source=root/'public-clean-v2/source/kde-index-source';original=root/'public-clean-v2/source/build/install_manifest.txt';before=hashlib.sha256(original.read_bytes()).hexdigest()
cmd=['bwrap','--die-with-parent','--unshare-all','--ro-bind',str(root),'/','--bind',str(base),'/public-stage-v3','--bind',str(manifest),'/public-clean-v2/source/build/install_manifest.txt','--ro-bind',str(P/'packaging/org.kdeindex.Search.metainfo.xml'),'/public-clean-v2/source/kde-index-source/packaging/org.kdeindex.Search.metainfo.xml','--proc','/proc','--dev','/dev','--tmpfs','/tmp','--clearenv','--setenv','PATH','/usr/bin','--setenv','DESTDIR','/public-stage-v3/stage','--chdir','/public-clean-v2/source/kde-index-source','/usr/bin/cmake','--install','../build','--prefix','/usr']
with (base/'install.log').open('wb') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60)
assert r.returncode==0 and hashlib.sha256(original.read_bytes()).hexdigest()==before
files={str(p.relative_to(base/'stage')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (base/'stage').rglob('*') if p.is_file()}
old={p['path']:p['sha256'] for p in json.loads((P/'evidence/public-clean-build-v2/stage-manifest.json').read_text())['files']}
changed=[f for f in old if old[f]!=files[f]];assert set(files)==set(old) and changed==['usr/share/metainfo/org.kdeindex.Search.metainfo.xml']
assert set(manifest.read_text().splitlines())=={'/public-stage-v3/stage/'+f for f in files}
record=dict(count=len(files),files=[dict(path=f,sha256=h,size=(base/'stage'/f).stat().st_size) for f,h in sorted(files.items())],manifest_matches_stage_paths=True,changed_from_clean_stage=changed,code_build_commit='5beacaebb65427478e7e2dd820c8cbea64cf13ea',command=cmd,exit_code=0,original_manifest_unchanged=True,scope='CMake staging only; updated metainfo data bound read-only, all20 other runtime/notice bytes unchanged; no host or bootstrap/usr installation')
(base/'stage-manifest.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'stage_count':len(files),'only_change':changed}))
