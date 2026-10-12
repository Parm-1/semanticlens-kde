"""Create a local publication Git root from the immutable RC source subset."""
from pathlib import Path
import hashlib,json,subprocess,tarfile
P=Path.cwd();out=P/'local/publication-v1';archive=P/'local/release-candidate-v2/kde-index-0.1.0-rc1-source.tar.gz'
receipt=json.loads((P/'local/release-candidate-v2/public-source-receipt.json').read_text())
assert hashlib.sha256(archive.read_bytes()).hexdigest()==receipt['archive_sha256']=='757543899ff3a231e1610d59829602a5166f768fef888e55d1a76a4e3524822d'
repo=out/'kde-index-source';assert not repo.exists()
modes={}
with tarfile.open(archive,'r:gz') as tf:
 for member in tf:
  assert member.isfile() and member.name.startswith('kde-index-source/')
  rel=member.name.removeprefix('kde-index-source/');assert rel in receipt['retained_files_sha256'] and rel not in modes
  modes[rel]=member.mode
 tf.extractall(out,filter='data')
expected=receipt['retained_files_sha256']
actual={str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in repo.rglob('*') if p.is_file()}
assert actual==expected and len(actual)==1991
assert all((repo/f).stat().st_mode&0o777==modes[f] for f in actual)
assert all(not (repo/f).exists() for f in receipt['omitted_system_binaries'])
def git(*args):
 return subprocess.check_output(['/usr/bin/git','-c','core.hooksPath=/dev/null',*args],cwd=repo)
git('init','--initial-branch=main');git('add','--force','--all')
assert set(git('ls-files','-z').decode().split('\0')[:-1])==set(expected)
git('-c','user.name=Codex','-c','user.email=codex@localhost','commit','-m','Prepare KDE Index 0.1.0-rc1 source')
head=git('rev-parse','HEAD').decode().strip();assert git('rev-list','--count','HEAD').strip()==b'1'
assert git('remote').strip()==b'' and git('status','--porcelain').strip()==b''
# The fresh repository is content-equivalent to the archive; no history/object
# reference connects it to the internal development repository.
rows=git('ls-tree','-rz','HEAD').split(b'\0');seen={}
for row in rows:
 if not row:continue
 header,name=row.split(b'\t',1);mode,kind,oid=header.split();name=name.decode();assert kind==b'blob'
 assert int(mode,8)&0o777==modes[name]
 payload=git('cat-file','blob',oid.decode());seen[name]=hashlib.sha256(payload).hexdigest()
assert seen==expected
record=dict(source_archive_sha256=receipt['archive_sha256'],internal_source_commit=receipt['internal_source_commit'],public_local_commit=head,branch='main',commit_count=1,tracked_files=len(seen),retained_content_and_modes_match=True,omitted_system_binaries_absent=True,remote_count=0,working_tree_clean=True,publication='not approved; no remote or upload',scope='Fresh local Git root; no internal history imported')
(out/'receipt.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
