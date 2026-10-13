#!/usr/bin/env python3
"""Fixed authored-pilot latency matrix on a fresh, explicitly selected service."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'service', 'service-sha256', 'tokenizer', 'tokenizer-sha256',
                 'probe', 'models', 'fixtures'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--prepare-only', action='store_true', help='Verify pins and freeze inputs without starting any process')
    parser.add_argument('--film-worker', help='Explicit optional shared-model worker; requires its SHA256')
    parser.add_argument('--film-worker-sha256')
    args = parser.parse_args()
    if bool(args.film_worker) != bool(args.film_worker_sha256):
        parser.error('--film-worker and --film-worker-sha256 must be supplied together')
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=False)
    service, tokenizer, probe = (Path(getattr(args, n)).resolve() for n in ('service', 'tokenizer', 'probe'))
    models, fixtures = Path(args.models).resolve(), Path(args.fixtures).resolve()
    pins = {service: args.service_sha256, tokenizer: args.tokenizer_sha256,
            probe: 'fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0',
            models/'embeddinggemma-2-Q8_0.gguf': '2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135',
            models/'mmproj-embeddinggemma-2-Q8_0.gguf': 'c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f',
            fixtures/'queries.json': 'fa5fbcdcd67580af62c4996cb8679f611dc89dae8038a1086a1dbc9883957c86'}
    film_worker = Path(args.film_worker).resolve() if args.film_worker else None
    if film_worker:
        pins[film_worker] = args.film_worker_sha256
    for path, expected in pins.items():
        if sha(path) != expected:
            raise ValueError('Pinned input mismatch: ' + str(path))
    queries = json.loads((fixtures/'queries.json').read_text())
    if len(queries) != 8:
        raise ValueError('Expected the unchanged eight-query pilot')
    corpus = out/'corpus'
    shutil.copytree(fixtures, corpus)
    data, sock = out/'data', out/'service.sock'
    if len(os.fsencode(sock)) > 100:
        raise ValueError('Choose a shorter absolute output path for the private Unix socket')
    source = corpus/'timing-load.txt'
    source.write_text(''.join('TIMING-LOAD-'+str(i)+': '+('The boat waits in the rain; the crew shares its work. '*10)+'\n' for i in range(2048)))
    manifest = json.loads((corpus/'harbor/manifest.json').read_text())
    manifest.update(film_id='film:timing-load', edition_id='edition:timing-load', display_title='Authored background timing load')
    manifest['sources'][0].update(source_id='source:timing-load', relative_path=source.name, sha256=sha(source))
    (corpus/'timing-load.json').write_text(json.dumps(manifest)+'\n')
    criterion = {'warm_target_ms': 1000, 'percentile': 95, 'algorithm': 'nearest rank: ceil(0.95*n)-1',
                 'matrix': 'eight fixed queries x source/hybrid x three repetitions, in declared order',
                 'samples_per_phase': 48, 'phases': ['warm_idle', 'warm_indexing'],
                 'indexing_eligibility': 'running embed job before AND after the measured interval',
                 'query_sha256': pins[fixtures/'queries.json'], 'clock': 'Python time.monotonic_ns',
                 'background_source_sha256': sha(source), 'background_manifest_sha256': sha(corpus/'timing-load.json'),
                 'interval': 'prepared request send through complete parsed reply; logs/phase checks outside',
                 'cold_query_scope': 'one source query after observed producer/tokenizer exit; separate from warm p95',
                 'resource_scope': 'phase snapshots of owned process RSS/HWM and data-file bytes, not peak workload memory',
                 'scope': 'authored six-edition/four-analysis CPU pilot; no real-film or bulk-library claim'}
    (out/'criterion.json').write_text(json.dumps(criterion, indent=2)+'\n')
    records, timings, checks = [], [], []
    report = {'criterion': criterion, 'pins': {str(p): h for p, h in pins.items()},
              'fixture_pins': {str(p.relative_to(fixtures)): sha(p) for p in sorted(fixtures.rglob('*')) if p.is_file()},
              'timings': timings, 'checks': checks, 'summary': {}, 'resource_closure': {}}
    report['resource_snapshots'] = []
    report['background_input'] = {'source_sha256': sha(source), 'manifest': manifest, 'lines': 2048}
    process, client, known = None, None, set()
    background_jid = None

    def check(name, passed, detail=None):
        checks.append({'name': name, 'passed': bool(passed), 'detail': detail})
        if not passed:
            raise AssertionError(name)

    class Client:
        def __init__(self):
            self.socket = socket.socket(socket.AF_UNIX)
            self.socket.settimeout(35)
            self.socket.connect(str(sock))
            self.buffer, self.sequence = b'', 0

        def call(self, op, measured=None, **fields):
            self.sequence += 1
            request = {'id': 'matrix-'+str(self.sequence), 'op': op, **fields}
            payload = (json.dumps(request, ensure_ascii=False)+'\n').encode()
            start = time.monotonic_ns() if measured else None
            self.socket.sendall(payload)
            while b'\n' not in self.buffer:
                chunk = self.socket.recv(1024*1024)
                if not chunk:
                    raise RuntimeError('Service EOF')
                self.buffer += chunk
            line, self.buffer = self.buffer.split(b'\n', 1)
            reply = json.loads(line)
            end = time.monotonic_ns() if measured else None
            records.append({'request': request, 'reply': reply})
            if measured:
                timings.append({**measured, 'request_id': request['id'], 'start_ns': start,
                                'end_ns': end, 'elapsed_ms': (end-start)/1e6})
            if reply.get('id') != request['id'] or not reply.get('ok'):
                raise RuntimeError('Invalid or unsuccessful reply: '+repr(reply))
            return reply

    def await_job(jid, wanted, seconds=120):
        deadline = time.monotonic()+seconds
        while time.monotonic() < deadline:
            job = client.call('film_status', job_id=jid)['status']['jobs'][0]
            if job['state'] in wanted:
                return job
            if job['state'] in ('failed', 'canceled', 'interrupted', 'complete'):
                raise RuntimeError('Unexpected terminal job: '+repr(job))
            time.sleep(.02)
        raise TimeoutError('Job '+jid)

    def import_job(op, relative):
        digest = client.call('film_sidecar_digest', collection_id=cid, relative_path=relative)['sha256']
        return client.call(op, collection_id=cid, relative_path=relative, expected_sha256=digest)['job_id']

    def children():
        result = set()
        for task in (Path('/proc')/str(process.pid)/'task').iterdir():
            try:
                result.update(int(pid) for pid in (task/'children').read_text().split())
            except FileNotFoundError:
                pass
        return sorted(result)

    def resource_snapshot(phase):
        rows = []
        for pid in [process.pid, *children()]:
            try:
                proc = Path('/proc')/str(pid)
                status = {}
                for line in (proc/'status').read_text().splitlines():
                    key, _, value = line.partition(':')
                    if key in ('VmRSS', 'VmHWM', 'PPid'):
                        status[key] = value.strip()
                rows.append({'pid': pid, 'exe': os.readlink(proc/'exe'), 'status': status})
            except (FileNotFoundError, ProcessLookupError):
                rows.append({'pid': pid, 'exited_during_snapshot': True})
        snapshot = {'phase': phase, 'monotonic_ns': time.monotonic_ns(), 'processes': rows,
                    'data_files': {str(p.relative_to(data)): p.stat().st_size
                                   for p in sorted(data.rglob('*')) if p.is_file()}}
        snapshot['data_bytes'] = sum(snapshot['data_files'].values())
        report['resource_snapshots'].append(snapshot)
        return snapshot

    def observed_idle_unload(phase):
        before = resource_snapshot(phase+' before unload')
        worker_paths = {str(probe), str(tokenizer)}
        if film_worker:
            worker_paths.add(str(film_worker))
        workers = [row['pid'] for row in before['processes']
                   if row.get('exe') in worker_paths]
        check(phase+' loaded worker observed', bool(workers), before)
        deadline = time.monotonic()+30
        while time.monotonic() < deadline and any(Path('/proc', str(pid)).exists() for pid in workers):
            time.sleep(.1)
        after = resource_snapshot(phase+' after unload')
        check(phase+' workers exited without another request',
              all(not Path('/proc', str(pid)).exists() for pid in workers)
              and not any(row.get('exe') in worker_paths for row in after['processes']), after)

    def matrix(phase, jid=None):
        for repetition in range(3):
            for mode in ('source', 'hybrid'):
                for query in queries:
                    before = client.call('film_status', job_id=jid)['status']['jobs'][0] if jid else None
                    reply = client.call('film_search', text=query['query'], mode=mode, collection_id=cid,
                                        include_unverified_prior=False,
                                        measured={'phase': phase, 'mode': mode, 'query_id': query['id'],
                                                  'repetition': repetition+1})
                    after = client.call('film_status', job_id=jid)['status']['jobs'][0] if jid else None
                    sample = timings[-1]
                    sample.update(before=before, after=after, result_editions=[r['edition_id'] for r in reply['results']])
                    sample['query_producer'] = reply.get('query_producer')
                    sample['eligible'] = jid is None or all(j['state']=='running' and j['stage']=='embed' for j in (before, after))
        samples = [x for x in timings if x['phase']==phase and x['eligible']]
        values = sorted(x['elapsed_ms'] for x in samples)
        check(phase+' complete declared matrix', len(samples)==48, len(samples))
        if film_worker:
            expected_provider = {
                'mode': 'shared_worker', 'sha256': pins[film_worker],
                'protocol': 'semanticlens-film-worker/1',
                'preprocessing': 'film-query-strict2048/1',
                'threads_total': 2, 'foreground_threads': 1, 'background_threads': 1,
                'allocation': 'reserved-foreground/1', 'ephemeral': True,
            }
            check(phase+' replies identify the selected query worker',
                  all(isinstance(sample.get('query_producer'), dict)
                      and all(sample['query_producer'].get(key) == value
                              for key, value in expected_provider.items())
                      for sample in samples), expected_provider)
        p95 = values[math.ceil(.95*len(values))-1]
        report['summary'][phase] = {'n': len(values), 'p95_ms': p95, 'max_ms': values[-1]}
        resource_snapshot('after '+phase+' matrix')
        check(phase+' p95 <= 1 second', p95 <= 1000, p95)

    argv = [str(service), '--data-dir', str(data), '--socket', str(sock), '--probe', str(probe),
            '--probe-sha256', pins[probe], '--text-model', str(models/'embeddinggemma-2-Q8_0.gguf'),
            '--image-model', str(models/'mmproj-embeddinggemma-2-Q8_0.gguf'), '--threads', '2',
            '--film-tokenizer', str(tokenizer), '--film-tokenizer-sha256', pins[tokenizer]]
    if film_worker:
        argv += ['--film-worker', str(film_worker), '--film-worker-sha256', pins[film_worker]]
    report['argv'] = argv
    if args.prepare_only:
        report['status'] = 'inputs_prepared_runtime_not_run'
        report['passed'] = None
        report['background_source_bytes'] = source.stat().st_size
        (out/'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
        print(json.dumps({'status': report['status'], 'background_source_bytes': source.stat().st_size}))
        return 0
    env = os.environ.copy()
    env.pop('LD_PRELOAD', None)
    env.pop('FILM_TRANSPORT_CAPTURE', None)
    log = (out/'service.log').open('wb')
    try:
        process = subprocess.Popen(argv, stdout=log, stderr=log, env=env, start_new_session=True)
        deadline = time.monotonic()+40
        while time.monotonic() < deadline:
            try:
                client = Client()
                break
            except (FileNotFoundError, ConnectionRefusedError):
                if process.poll() is not None:
                    raise RuntimeError('Service exited at startup')
                time.sleep(.05)
        if client is None:
            raise TimeoutError('Service socket')
        deadline = time.monotonic()+60
        while time.monotonic() < deadline:
            status = client.call('status')['status']
            if status['ready']:
                break
            time.sleep(.04)
        check('legacy ready, producer unchanged', status['ready'] and status['identity']['producer_sha256']==pins[probe])
        report['identity'] = status['identity']
        capability = client.call('film_capabilities')
        check('qualified default-exclusion ranking policy', capability['ranking_version']=='semanticlens-film-rrf/2'
              and capability['search_options']['include_unverified_prior']['default'] is False)
        cid = client.call('film_add_collection', path=str(corpus))['collection_id']
        for name in ('harbor', 'harbor-cut', 'orchard', 'crossing', 'archive', 'parcel-pdf'):
            jid = import_job('film_import_manifest', name+'/manifest.json')
            check(name+' source imported', await_job(jid, ('complete',))['state']=='complete')
        for name in ('harbor', 'harbor-cut', 'orchard', 'crossing'):
            jid = import_job('film_import_analysis', name+'/analysis.json')
            check(name+' analysis imported', await_job(jid, ('complete',))['state']=='complete')
        check('unchanged pilot active counts', client.call('film_status')['status']['counts']['editions']==6)
        observed_idle_unload('pilot idle')
        cold_reply = client.call('film_search', text=queries[0]['query'], mode='source', collection_id=cid,
                                 include_unverified_prior=False,
                                 measured={'phase': 'cold_reload', 'mode': 'source', 'query_id': queries[0]['id']})
        report['summary']['cold_reload'] = {'n': 1, 'elapsed_ms': timings[-1]['elapsed_ms'],
                                          'result_editions': [r['edition_id'] for r in cold_reply['results']]}
        resource_snapshot('after cold reload')
        for mode in ('source', 'hybrid'):
            for query in queries:
                client.call('film_search', text=query['query'], mode=mode, collection_id=cid)
        matrix('warm_idle')
        jid = import_job('film_import_manifest', 'timing-load.json')
        background_jid = jid
        deadline = time.monotonic()+120
        while time.monotonic() < deadline:
            job = client.call('film_status', job_id=jid)['status']['jobs'][0]
            if job['state']=='running' and job['stage']=='embed':
                break
            time.sleep(.02)
        check('background embedding stage observed', job['state']=='running' and job['stage']=='embed', job)
        matrix('warm_indexing', jid)
        client.call('film_cancel_job', job_id=jid)
        check('background job canceled', await_job(jid, ('canceled',))['state']=='canceled')
        check('canceled staging preserves six active editions', client.call('film_status')['status']['counts']['editions']==6)
        observed_idle_unload('post-cancel idle')
        client.call('shutdown')
        process.wait(timeout=15)
        check('orderly shutdown exit0', process.returncode == 0, process.returncode)
    except Exception as error:
        report['failure'] = repr(error)
        # The measured interval has ended. Try the real control path before
        # forced group cleanup, and retain cleanup errors separately from the
        # original failed assertion/response.
        report['failure_cleanup'] = {}
        if client and process and process.poll() is None:
            try:
                if background_jid:
                    report['failure_cleanup']['cancel_reply'] = client.call('film_cancel_job', job_id=background_jid)
                report['failure_cleanup']['shutdown_reply'] = client.call('shutdown')
                process.wait(timeout=15)
            except Exception as cleanup_error:
                report['failure_cleanup']['error'] = repr(cleanup_error)
    finally:
        if client:
            client.socket.close()
        if process:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=10)
            # Give actual child exits/reaping a bounded interval before the
            # final receipt; do not call an instantaneous exit race closure.
            deadline = time.monotonic()+5
            while True:
                known.clear()
                for entry in Path('/proc').iterdir():
                    if not entry.name.isdecimal():
                        continue
                    try:
                        raw = (entry/'stat').read_text()
                        fields = raw[raw.rfind(')')+2:].split()
                        if int(fields[2])==process.pid:
                            known.add(int(entry.name))
                    except (FileNotFoundError, ProcessLookupError, PermissionError):
                        pass
                if not known or time.monotonic()>=deadline:
                    break
                time.sleep(.05)
            report['resource_closure'] = {'service_exit': process.returncode, 'socket_absent': not sock.exists(),
                                          'remaining_owned_process_group': sorted(known)}
        log.close()
        report['passed'] = not report.get('failure') and all(x['passed'] for x in checks) and not known and not sock.exists()
        (out/'records.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')
        (out/'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'passed': report['passed'], 'summary': report['summary'], 'failure': report.get('failure')}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
