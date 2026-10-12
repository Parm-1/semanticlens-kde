import hashlib, json, subprocess, time
from pathlib import Path
out = Path('/public-parity-v2')
probe = Path('/public-clean-v2/stage/usr/bin/embedding-probe')
models = Path('/public-parity-v2/models')
cmd = [str(probe), str(models/'embeddinggemma-2-Q8_0.gguf'), str(models/'mmproj-embeddinggemma-2-Q8_0.gguf'), '6']
meta = {'binary_sha256': hashlib.sha256(probe.read_bytes()).hexdigest(), 'runs': [], 'scope': 'Actual exact-v2 clean source-archive-built CPU producer; diagnostic correctness only, not performance.'}
for name, extra, source, timeout in [('run', [], 'requests.jsonl', 150), ('query-only', ['--text-only'], 'query-only.jsonl', 60)]:
    begin = time.monotonic_ns()
    with (out/'inputs'/source).open('rb') as inp, (out/(name+'.jsonl')).open('wb') as stdout, (out/(name+'.log')).open('wb') as stderr:
        result = subprocess.run(cmd + extra, stdin=inp, stdout=stdout, stderr=stderr, timeout=timeout)
    end = time.monotonic_ns()
    meta['runs'].append({'name': name, 'command': cmd+extra, 'exit_code': result.returncode, 'begin_ns': begin, 'end_ns': end})
    if result.returncode:
        raise SystemExit(f'{name} exited {result.returncode}')
(out/'run-time.json').write_text(json.dumps(meta, indent=2)+'\n')
