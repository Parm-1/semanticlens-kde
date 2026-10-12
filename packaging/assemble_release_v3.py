#!/usr/bin/env python3
"""Assemble a public-source subset and exact runtime stage with build correspondence."""
from __future__ import annotations
import argparse,json,re,shutil,tempfile
from pathlib import Path
from assemble_release_v2 import ROOT,run_git,git_commit,git_tree,native_inventory,add_native,verify_native_archive,sha256_file
from export_public_source_v1 import export

def metadata_only(path: str) -> bool:
 return path in ('GOAL.md','README.md','packaging/org.kdeindex.Search.metainfo.xml','.github/workflows/build.yml') or path.startswith(('docs/','evidence/')) or bool(re.fullmatch(r'packaging/(?:assemble_release_v3|export_public_source_v1)\.py',path))

def assemble(source: str,build: str,stage: Path,manifest: Path,output: Path,version: str) -> dict:
 source=git_commit(source);build=git_commit(build)
 if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.+_-]{0,63}',version):raise ValueError('unsafe candidate label')
 if run_git('merge-base',build,source).decode().strip()!=build:raise ValueError('build must be a source ancestor')
 changes=[p for p in run_git('diff','--name-only','-z',build,source).decode().split('\0') if p]
 blocked=[p for p in changes if not metadata_only(p)]
 if blocked:raise ValueError('native build inputs changed since build: '+', '.join(blocked[:8]))
 record=json.loads(manifest.read_text());expected={p['path'].removeprefix('usr/'):p['sha256'] for p in record['files']}
 if len(expected)!=record['count'] or not record['manifest_matches_stage_paths']:raise ValueError('invalid stage manifest')
 if output.exists():raise ValueError('output must be new')
 output.parent.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='.public-release-',dir=output.parent))
 try:
  # Adapt the already pinned public-clean stage schema to the existing strict
  # allowlist/notice-to-source validator; do not rewrite the original receipt.
  adapter=work/'stage-adapter.json';adapter.write_text(json.dumps({'actual_new':expected}))
  native=native_inventory(stage.resolve(),adapter,source,git_tree(source));adapter.unlink()
  # Metainfo is an installed data file, separately traced after the code build.
  if (stage/'share/metainfo/org.kdeindex.Search.metainfo.xml').read_bytes()!=run_git('show',source+':packaging/org.kdeindex.Search.metainfo.xml'):
   raise ValueError('staged metainfo differs from selected source')
  exported=export(source,work/'source-export')
  native_name=f'kde-index-{version}-native.tar.gz';prefix=f'kde-index-{version}-native'
  add_native(stage.resolve(),prefix,work/native_name,exported['source_date_epoch'],native['files'])
  verify_native_archive(work/native_name,prefix,native['files'])
  source_name=f'kde-index-{version}-source.tar.gz'
  shutil.move(str(work/'source-export/kde-index-source.tar.gz'),work/source_name)
  shutil.move(str(work/'source-export/receipt.json'),work/'public-source-receipt.json');(work/'source-export').rmdir()
  hashes={source_name:sha256_file(work/source_name),native_name:sha256_file(work/native_name)}
  receipt=dict(format=3,candidate_version=version,source_commit=source,native_build_commit=build,
   source_epoch=exported['source_date_epoch'],source_build_changed_metadata_paths=changes,
   public_source_files=exported['included_files'],omitted_system_binaries=exported['omitted_system_binaries'],
   native_stage_file_count=len(native['files']),native_license_file_count=native['license_file_count'],
   native_stage_files_sha256=native['files'],archives_sha256=hashes,
   limits=['Local candidate assembly only; no publication or final release acceptance.',
    'The public source is a declared subset of internal evidence; four historical system files omitted, original manifests unchanged.',
    'Native payload is the exact tested Arch x86_64 CPU stage, requiring target system libraries and its recorded CPU instructions.',
    'Metadata-only changes after the native build are listed; installed notices must still byte-match the selected source.',
    'Assembly/readback does not independently prove runtime, general platform compatibility, privacy or license compliance.'])
  (work/'package-receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
  (work/'SHA256SUMS').write_text(''.join(f'{hashes[n]}  {n}\n' for n in sorted(hashes)))
  work.rename(output)
 except BaseException:
  shutil.rmtree(work,ignore_errors=True);raise
 return receipt

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('commit','build-commit','version'):p.add_argument('--'+name,required=True)
 for name in ('stage','stage-manifest','output'):p.add_argument('--'+name,required=True,type=Path)
 a=p.parse_args();r=assemble(a.commit,a.build_commit,a.stage,a.stage_manifest,a.output.resolve(),a.version)
 print(json.dumps({k:r[k] for k in ('source_commit','native_build_commit','public_source_files','native_stage_file_count','archives_sha256')},indent=2))
