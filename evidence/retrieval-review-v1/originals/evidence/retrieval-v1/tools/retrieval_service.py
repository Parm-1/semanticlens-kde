#!/usr/bin/env python3
"""Evaluate frozen synthetic inputs on an explicit, immutable service candidate."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import socket
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def descendants(pid):
    found, pending = set(), [pid]
    while pending:
        current = pending.pop()
        if current in found:
            continue
        found.add(current)
        for children in Path(f'/proc/{current}/task').glob('*/children'):
            try:
                pending.extend(map(int, children.read_text().split()))
            except FileNotFoundError:
                pass
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('service', 'probe', 'models', 'output', 'candidate-pins'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    service, probe, models, output, pins = [p.resolve() for p in
        (args.service, args.probe, args.models, args.output, args.candidate_pins)]
    if output.exists():
        parser.error('preserve prior results; supply a new output directory')
    subprocess.run(['sha256sum', '-c', str(pins)], cwd=pins.parent, check=True,
                   stdout=subprocess.DEVNULL)
    if digest(service) not in {line.split()[0] for line in pins.read_text().splitlines()}:
        parser.error('service executable is not bound by the verified candidate manifest')
    subprocess.run(['sha256sum', '-c', 'benchmark/SHA256SUMS'], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    specification = json.loads((ROOT / 'benchmark/queries.json').read_text())
    output.mkdir(parents=True)
    corpus, data = output / 'corpus', output / 'data'
    shutil.copytree(ROOT / 'benchmark/corpus', corpus)
    socket_path = f'/tmp/kde-index-retrieval-{os.getpid()}'
    command = [str(service), '--data-dir', str(data), '--socket', socket_path,
               '--probe', str(probe), '--probe-sha256', digest(probe),
               '--text-model', str(models / 'embeddinggemma-2-Q8_0.gguf'),
               '--image-model', str(models / 'mmproj-embeddinggemma-2-Q8_0.gguf')]
    report = {'schema': 1, 'scope': 'frozen authored synthetic corpus v1',
              'service_sha256': digest(service), 'probe_sha256': digest(probe),
              'candidate_pins_sha256': digest(pins),
              'benchmark_pins_sha256': digest(ROOT / 'benchmark/SHA256SUMS'),
              'queries': [], 'warm_repeats': [], 'cold_queries': [],
              'latency_scope': 'monotonic send through matching decoded reply; includes IPC and inference; 5 ordered repeats of all 37 queries',
              'rss_scope': '50ms /proc sampler, sum of resident KiB of service and live descendants; shared pages counted per process; sampled peak, not unique memory',
              'known_pids': [], 'sampled_peak_rss_kib': 0}
    log = (output / 'service.log').open('wb')
    proc = subprocess.Popen(command, stdout=log, stderr=log)
    stop_sample = threading.Event()
    known = {proc.pid}

    def sample():
        while not stop_sample.is_set():
            total = 0
            pids = descendants(proc.pid)
            known.update(pids)
            for pid in pids:
                try:
                    for line in Path(f'/proc/{pid}/status').read_text().splitlines():
                        if line.startswith('VmRSS:'):
                            total += int(line.split()[1])
                except FileNotFoundError:
                    pass
            report['sampled_peak_rss_kib'] = max(report['sampled_peak_rss_kib'], total)
            stop_sample.wait(.05)

    sampler = threading.Thread(target=sample, daemon=True)
    sampler.start()
    conn, buffer, sequence = None, b'', 0

    def request(op, **values):
        nonlocal buffer, sequence
        sequence += 1
        identity = str(sequence)
        payload = json.dumps({'id': identity, 'op': op, **values}).encode() + b'\n'
        begin = time.monotonic_ns()
        conn.sendall(payload)
        while b'\n' not in buffer:
            chunk = conn.recv(65536)
            if not chunk:
                raise RuntimeError('service EOF')
            buffer += chunk
            if len(buffer) > 4 * 1024 * 1024:
                raise RuntimeError('response exceeds bounded buffer')
        line, buffer = buffer.split(b'\n', 1)
        reply = json.loads(line)
        end = time.monotonic_ns()
        if reply.get('protocol') != 1 or reply.get('id') != identity or not reply.get('ok'):
            raise RuntimeError(f'unexpected reply: {reply}')
        return reply, {'begin_ns': begin, 'end_ns': end, 'elapsed_ms': (end - begin) / 1e6}

    def wait_status(predicate, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            status = request('status')[0]['status']
            if status.get('error'):
                raise RuntimeError(status['error'])
            if predicate(status):
                return status
            time.sleep(.1)
        raise TimeoutError(f'status deadline: {status}')

    def search(entry):
        reply, measurement = request('search', text=entry['text'], limit=5)
        rows = reply['results']
        if len(rows) > 5 or len({r['path'] for r in rows}) != len(rows):
            raise RuntimeError('not a distinct-file limit-five result')
        hits = []
        for row in rows:
            file = Path(row['path']).relative_to(corpus).as_posix()
            if not math.isfinite(row['score']):
                raise RuntimeError('nonfinite score')
            hits.append({**row, 'path': file, 'file': file})
        return {'id': entry['id'], 'query': entry['text'], 'hits': hits,
                'index_revision': reply['index_revision'], **measurement}

    try:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                raise RuntimeError('service exited during startup')
            conn = socket.socket(socket.AF_UNIX)
            try:
                conn.connect(socket_path)
                conn.settimeout(120)
                break
            except OSError:
                conn.close(); conn = None; time.sleep(.05)
        if conn is None:
            raise TimeoutError('socket startup')
        wait_status(lambda s: s['ready'], 30)
        begin = time.monotonic_ns()
        request('add_root', path=str(corpus))
        status = wait_status(lambda s: not s['busy'] and s['indexed'] == 28, 180)
        end = time.monotonic_ns()
        if status['errors'] or status['failures']:
            raise RuntimeError(f'extraction incomplete: {status}')
        report['indexing'] = {'begin_ns': begin, 'end_ns': end,
                              'elapsed_seconds': (end - begin) / 1e9, 'status': status}
        report['queries'] = [search(q) for q in specification['queries']]
        for repeat in range(5):
            for entry in specification['queries']:
                row = search(entry)
                report['warm_repeats'].append({'repeat': repeat, 'id': entry['id'],
                                              'begin_ns': row['begin_ns'], 'end_ns': row['end_ns'],
                                              'elapsed_ms': row['elapsed_ms']})
        times = sorted(r['elapsed_ms'] for r in report['warm_repeats'])
        report['warm_p95_ms'] = times[math.ceil(.95 * len(times)) - 1]
        # Three fixed modalities, chosen before observing any retrieval output.
        for identity in ('s01', 's09', 's13'):
            deadline = time.monotonic() + 20
            while descendants(proc.pid) != {proc.pid}:
                if time.monotonic() >= deadline:
                    raise TimeoutError('heavy worker did not leave at idle')
                time.sleep(.05)
            entry = next(q for q in specification['queries'] if q['id'] == identity)
            report['cold_queries'].append({'producer_absent_before': True, **search(entry)})
        report['index_size_bytes'] = sum(p.stat().st_size for p in data.glob('index.sqlite*'))
        request('shutdown')
        conn.close(); conn = None
        proc.wait(timeout=15)
        if proc.returncode != 0:
            raise RuntimeError(f'service exit {proc.returncode}')
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
    finally:
        if conn:
            conn.close()
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(timeout=5)
        stop_sample.set(); sampler.join(timeout=2); log.close()
        deadline = time.monotonic() + 10
        while any(Path(f'/proc/{pid}').exists() for pid in known) and time.monotonic() < deadline:
            time.sleep(.1)
        report['known_pids'] = sorted(known)
        report['closure'] = {'service_exit': proc.returncode,
                             'all_known_pids_absent': all(not Path(f'/proc/{p}').exists() for p in known),
                             'socket_absent': not Path(socket_path).exists()}
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    subprocess.run(['sha256sum', '-c', 'benchmark/SHA256SUMS'], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    return int('error' in report or not report['closure']['all_known_pids_absent'] or
               not report['closure']['socket_absent'])


if __name__ == '__main__':
    raise SystemExit(main())
