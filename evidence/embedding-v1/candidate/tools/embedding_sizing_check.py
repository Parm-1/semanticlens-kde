#!/usr/bin/env python3
"""Checks four source-frozen PIL byte expectations against the C++ resize port."""
import json,re,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1] if len(sys.argv)>1 else 'evidence/embedding-v1')
if (root/'sizing-results.json').exists():raise SystemExit('Sizing results exist. Use a successor evidence version.')
s=(root/'source/ImageSizingTests.swift').read_text()
source=list(map(int,re.search(r'static let source: \[UInt8\] = \[(.*?)\]',s,re.S)[1].replace('\n','').split(',')))
cases=[]
for method,w,h in [('shrinkMatchesPillow',4,3),('growMatchesPillow',10,6),('oneAxisMatchesPillow',7,2)]:
 block=s[s.index('func '+method):]
 expected=list(map(int,re.search(r'let pil: \[UInt8\] = \[(.*?)\]',block,re.S)[1].replace('\n','').split(',')))
 cases.append((method,w,h,expected))
cases.append(('identity',7,5,source))
requests=[dict(source=source,width=7,height=5,out_width=w,out_height=h) for _,w,h,_ in cases]
(root/'inputs/sizing.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in requests))
p=subprocess.run(['build-probe/sizing-check'],input=''.join(json.dumps(r)+'\n' for r in requests),text=True,capture_output=True,check=True)
out=[json.loads(line) for line in p.stdout.splitlines()]
results=[dict(name=name,bytes=len(got),matches=got==expected) for (name,_,_,expected),got in zip(cases,out)]
(root/'sizing-results.json').write_text(json.dumps(results,indent=2)+'\n')
assert len(out)==4 and all(r['matches'] for r in results)
print(json.dumps(results))
