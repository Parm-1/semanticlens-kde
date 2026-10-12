import hashlib,json,resource,subprocess,time
from pathlib import Path
out=Path('local/installed-producer-parity-v2')
assert not (out/'run.jsonl').exists()
inputs=Path('evidence/embedding-v1/inputs')
(out/'inputs').symlink_to(Path('../../evidence/embedding-v1/inputs'))
producer=Path('local/stage-v2/usr/bin/embedding-probe')
cmd=[str(producer),'models/embeddinggemma-2-Q8_0.gguf','models/mmproj-embeddinggemma-2-Q8_0.gguf','6']
start=time.perf_counter()
with (inputs/'requests.jsonl').open('rb') as i,(out/'run.jsonl').open('wb') as o,(out/'run.log').open('wb') as e:
 p=subprocess.run(cmd,stdin=i,stdout=o,stderr=e,timeout=150)
meta={'command':cmd,'binary_sha256':hashlib.sha256(producer.read_bytes()).hexdigest(),'exit_code':p.returncode,'elapsed_seconds':time.perf_counter()-start,'peak_rss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'scope':'One producer child in fresh Arch userspace sharing host kernel and CPU. Concurrent engineering work may affect timing. Frozen upstream inputs unchanged.'}
(out/'run-time.json').write_text(json.dumps(meta,indent=2)+'\n')
assert p.returncode==0
with (inputs/'query-only.jsonl').open('rb') as i,(out/'query-only.jsonl').open('wb') as o,(out/'query-only.log').open('wb') as e:
 p=subprocess.run(cmd+['--text-only'],stdin=i,stdout=o,stderr=e,timeout=60)
assert p.returncode==0
