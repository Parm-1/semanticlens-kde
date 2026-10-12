import hashlib,json,resource,subprocess,time
from pathlib import Path
out=Path('/archive-source-v1/producer-parity-v1');out.mkdir(exist_ok=False)
(out/'inputs').symlink_to('../kde-index-0.1.0-dev-archivecheck-source/evidence/embedding-v1/inputs')
probe=Path('/archive-source-v1/stage/usr/bin/embedding-probe')
cmd=[str(probe),'models/embeddinggemma-2-Q8_0.gguf','models/mmproj-embeddinggemma-2-Q8_0.gguf','6']
meta={'binary_sha256':hashlib.sha256(probe.read_bytes()).hexdigest(),'runs':[],'scope':'Actual source-archive-built producer in owned Arch userspace; diagnostic correctness, no performance claim.'}
for name,extra,bound in [('run',[],150),('query-only',['--text-only'],60)]:
 source='requests.jsonl' if name=='run' else 'query-only.jsonl';start=time.monotonic_ns()
 with (out/'inputs'/source).open('rb') as i,(out/(name+'.jsonl')).open('wb') as o,(out/(name+'.log')).open('wb') as e:r=subprocess.run(cmd+extra,stdin=i,stdout=o,stderr=e,timeout=bound)
 meta['runs'].append({'name':name,'command':cmd+extra,'exit_code':r.returncode,'begin_ns':start,'end_ns':time.monotonic_ns()});assert r.returncode==0
(out/'run-time.json').write_text(json.dumps(meta,indent=2)+'\n')
