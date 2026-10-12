"""Check-only linked receipt for the already completed CMake staging operation."""
import hashlib,json
from pathlib import Path
P=Path.cwd();root=(P/'local/clean-arch-v1/root.x86_64').resolve();base=root/'public-stage-v3'
files={str(p.relative_to(base/'stage')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (base/'stage').rglob('*') if p.is_file()}
old={p['path']:p['sha256'] for p in json.loads((P/'evidence/public-clean-build-v2/stage-manifest.json').read_text())['files']}
changed=[f for f in old if old[f]!=files[f]];assert set(files)==set(old) and changed==['usr/share/metainfo/org.kdeindex.Search.metainfo.xml']
assert set((base/'install_manifest.txt').read_text().splitlines())=={'/'+f for f in files}
assert (base/'stage/usr/share/metainfo/org.kdeindex.Search.metainfo.xml').read_bytes()==(P/'packaging/org.kdeindex.Search.metainfo.xml').read_bytes()
record=dict(count=len(files),files=[dict(path=f,sha256=h,size=(base/'stage'/f).stat().st_size) for f,h in sorted(files.items())],manifest_matches_stage_paths=True,manifest_prefix='/usr; CMake omits DESTDIR in its manifest',changed_from_clean_stage=changed,code_build_commit='5beacaebb65427478e7e2dd820c8cbea64cf13ea',scope='Check-only receipt for completed CMake staging; all20 other runtime/notice bytes unchanged. Original harness passed operation-exit and frozen-manifest checks before failing its DESTDIR interpretation; no staging rerun here.')
(base/'stage-manifest.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'stage_count':len(files),'only_change':changed,'check_only':True}))
