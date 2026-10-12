#!/usr/bin/env python3
"""Run one bounded worker process; sole-child resource scope and steady elapsed clock."""
import json,resource,subprocess,sys,time
from pathlib import Path
if len(sys.argv)<3:raise SystemExit('usage: embedding_run.py INPUT.jsonl OUTPUT-STEM [--text-only]')
source=Path(sys.argv[1]);stem=Path(sys.argv[2])
paths=[Path(str(stem)+suffix) for suffix in ('.jsonl','.log','-time.json')]
if any(p.exists() for p in paths):raise SystemExit('Output exists; use a new evidence version or stem.')
cmd=['build-probe/embedding-probe','models/embeddinggemma-2-Q8_0.gguf','models/mmproj-embeddinggemma-2-Q8_0.gguf','6']+sys.argv[3:]
start=time.perf_counter()
with source.open('rb') as input_,paths[0].open('wb') as output,paths[1].open('wb') as log:
 result=subprocess.run(cmd,stdin=input_,stdout=output,stderr=log)
elapsed=time.perf_counter()-start
usage=resource.getrusage(resource.RUSAGE_CHILDREN)
paths[2].write_text(json.dumps(dict(command=cmd,exit_code=result.returncode,elapsed_seconds=elapsed,user_seconds=usage.ru_utime,system_seconds=usage.ru_stime,peak_rss_kib=usage.ru_maxrss,scope='One child process in a fresh Python runner; Linux ru_maxrss KiB; perf_counter start before spawn to after wait. Includes load, warmup, requests and teardown; no memory sample profiler.'),indent=2)+'\n')
raise SystemExit(result.returncode)
