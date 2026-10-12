#!/usr/bin/env python3
"""Export a pinned source snapshot without unnecessary copied system binaries."""
from __future__ import annotations
import argparse,gzip,hashlib,io,json,subprocess,tarfile
from pathlib import Path
from assemble_release_v2 import ROOT,run_git,git_commit,git_tree,check_payload,normalize_member

# Frozen internal proof stays byte-identical. Public distribution omits only these
# foreign executables, preserving their identity receipts and original manifests.
OMITTED = {
 'evidence/desktop-open-v3/candidate/native-bin/cat':'b97039a4875fc8d2262bb78523032f5b7637308da54551fb55038c38274817be',
 'evidence/desktop-open-v3/candidate/native-bin/konsole':'b499a7e7c656b52da664686b532f152e3ba78a51f37aaa8e6894de39f9bb8dba',
 'evidence/desktop-open-v3/candidate/native-bin/libkonsoleapp.so.26.08.2':'c737985078bad3de74bad38c278a9ffa30dfb53746529c7a425831c9d25f7c86',
 'evidence/desktop-open-v3/candidate/native-bin/libkonsoleprivate.so.26.08.2':'0beaf582a41ff1b97d5b4787fd49ca489f6f31776a6e8bc393920056899b953a',
}

def export(commit: str, output: Path) -> dict:
 commit=git_commit(commit);tree=git_tree(commit)
 if not set(OMITTED)<=set(tree):raise ValueError('pinned internal tree lacks expected omitted objects')
 if any(mode not in ('100644','100755') for mode,oid in tree.values()):
  raise ValueError('this export profile supports regular Git files only')
 output.mkdir(exist_ok=False);archive=output/'kde-index-source.tar.gz'
 epoch=int(run_git('show','-s','--format=%ct',commit).strip());prefix='kde-index-source'
 included={};omitted={}
 with archive.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=epoch,compresslevel=9) as gz,tarfile.open(fileobj=gz,mode='w|',format=tarfile.GNU_FORMAT) as dst:
  proc=subprocess.Popen(['/usr/bin/git','archive','--format=tar',commit],cwd=ROOT,stdout=subprocess.PIPE)
  try:
   with tarfile.open(fileobj=proc.stdout,mode='r|') as src:
    for member in src:
     if member.isdir():continue
     name=member.name
     if name not in tree or not member.isfile():raise ValueError('unexpected source object: '+name)
     payload=src.extractfile(member).read();oid=hashlib.sha1(b'blob '+str(len(payload)).encode()+b'\0'+payload).hexdigest()
     if oid!=tree[name][1]:raise ValueError('Git blob mismatch: '+name)
     digest=hashlib.sha256(payload).hexdigest()
     if name in OMITTED:
      if digest!=OMITTED[name]:raise ValueError('omitted artifact identity changed: '+name)
      omitted[name]=digest;continue
     check_payload(name,payload)
     included[name]=digest
     member.mode=int(tree[name][0],8)&0o777
     member.name=prefix+'/'+name;dst.addfile(normalize_member(member,epoch),io.BytesIO(payload))
   if proc.wait()!=0:raise ValueError('git archive failed')
  finally:
   if proc.poll() is None:proc.kill();proc.wait()
 if set(included)!=set(tree)-set(OMITTED) or omitted!=OMITTED:raise ValueError('export tree differs from policy')
 readback={}
 with tarfile.open(archive,'r:gz') as src:
  for member in src:
   if not member.isfile() or not member.name.startswith(prefix+'/'):raise ValueError('unexpected exported member')
   name=member.name[len(prefix)+1:]
   if name in readback:raise ValueError('duplicate exported member')
   if member.mode != int(tree[name][0],8)&0o777:raise ValueError('exported mode differs from Git tree')
   readback[name]=hashlib.sha256(src.extractfile(member).read()).hexdigest()
 if readback!=included:raise ValueError('export archive readback mismatch')
 receipt=dict(profile='public-source-v1',internal_source_commit=commit,source_date_epoch=epoch,
  archive=archive.name,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),included_files=len(included),
  omitted_system_binaries=omitted,retained_files_sha256=included,
  evidence_limit='Original manifests remain unchanged and reference four intentionally omitted files. The exported subset is not the complete internal evidence packet. Retained bytes and hashes are preserved; re-running complete original manifests requires the internal packet or independently supplied matching system artifacts.',
  publication='local preparation only; no repository identity or publication approved')
 (output/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
 return {k:v for k,v in receipt.items() if k!='retained_files_sha256'}

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--commit',required=True);parser.add_argument('--output',required=True,type=Path)
 args=parser.parse_args();print(json.dumps(export(args.commit,args.output.resolve()),indent=2))
