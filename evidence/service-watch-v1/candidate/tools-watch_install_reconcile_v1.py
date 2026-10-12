#!/usr/bin/env python3
import hashlib, json, os, re, socket, subprocess, time
from pathlib import Path

P = Path(__file__).resolve().parent.parent
O = P / 'local/build-own/service-watch-v1'
BIN = O / 'build'
PROBE = P / 'local/clean-arch-v1/root.x86_64/checkout/local/stage-v5/usr/bin/embedding-probe'
TEXT_MODEL = P / 'models/embeddinggemma-2-Q8_0.gguf'
IMAGE_MODEL = P / 'models/mmproj-embeddinggemma-2-Q8_0.gguf'
TOKEN = 'WATCH-INSTALL-RECONCILE-60431'
BODY = TOKEN + ' automatically discovered file body'


def tree(pid):
    out = set()
    todo = [pid]
    while todo:
        parent = todo.pop()
        try:
            kids = Path(f'/proc/{parent}/task/{parent}/children').read_text().split()
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            continue
        for child in map(int, kids):
            if child not in out:
                out.add(child)
                todo.append(child)
    return out


def run_case(label, binary, timeout):
    root = O / (label + '-root')
    data = O / (label + '-data')
    sockpath = O / (label + '.sock')
    for path in (root, data):
        if path.exists():
            import shutil
            shutil.rmtree(path)
    if sockpath.exists():
        sockpath.unlink()
    root.mkdir()
    (root / 'source.txt').write_text('ORIGINAL-OWNED-92111 initial source content')
    log = (O / (label + '.log')).open('wb')
    cmd = [str(binary), '--data-dir', str(data), '--socket', str(sockpath),
           '--probe', str(PROBE), '--probe-sha256', hashlib.sha256(PROBE.read_bytes()).hexdigest(),
           '--text-model', str(TEXT_MODEL), '--image-model', str(IMAGE_MODEL), '--threads', '2']
    proc = subprocess.Popen(cmd, stdout=log, stderr=log)
    records = []
    buf = b''
    rid = 0
    c = None

    def req(op, **kw):
        nonlocal buf, rid
        rid += 1
        c.sendall(json.dumps(dict(op=op, id=str(rid), **kw)).encode() + b'\n')
        while b'\n' not in buf:
            chunk = c.recv(65536)
            if not chunk:
                raise RuntimeError('service socket closed while awaiting ' + op)
            buf += chunk
        line, buf = buf.split(b'\n', 1)
        r = json.loads(line)
        if r.get('id') != str(rid):
            raise RuntimeError('mismatched response id')
        return r

    inode = root.stat().st_ino

    def witness():
        raw = {}
        try:
            for f in Path(f'/proc/{proc.pid}/fdinfo').iterdir():
                value = f.read_text()
                if 'inotify wd:' in value:
                    raw[f.name] = value
        except (FileNotFoundError, ProcessLookupError):
            pass
        found = any(int(match, 16) == inode
                    for value in raw.values()
                    for match in re.findall(r'inotify wd:\d+ ino:([0-9a-fA-F]+)', value))
        return {'clock_ns': time.monotonic_ns(), 'root_inode': inode,
                'root_watch_installed': found, 'raw': raw}

    try:
        for _ in range(500):
            if proc.poll() is not None:
                raise RuntimeError(f'{label} service exited early: {proc.returncode}')
            try:
                c = socket.socket(socket.AF_UNIX)
                c.settimeout(40)
                c.connect(str(sockpath))
                break
            except OSError:
                if c:
                    c.close()
                c = None
                time.sleep(.02)
        if c is None:
            raise RuntimeError('service socket did not appear')
        for _ in range(1500):
            s = req('status')['status']
            if s.get('ready'):
                break
            time.sleep(.02)
        else:
            raise RuntimeError('service did not become ready')

        req('add_root', path=str(root))
        before = witness()
        create_before = time.monotonic_ns()
        (root / 'auto.txt').write_text(BODY)
        create_after = time.monotonic_ns()
        deadline = time.monotonic() + timeout
        observed = None
        while time.monotonic() < deadline:
            observed = req('status')['status']
            if observed.get('ready') and observed.get('indexed', 0) >= 2 and not observed.get('busy'):
                break
            time.sleep(.08)
        after = witness()
        query = req('search', text=TOKEN)
        rows = query.get('results', [])
        auto_rows = [r for r in rows if r.get('path') == str(root / 'auto.txt') and BODY in r.get('text', '')]
        pre_scan_positive = bool(auto_rows)
        explicit = None
        if label == 'original':
            req('scan')
            for _ in range(500):
                final = req('status')['status']
                if final.get('ready') and not final.get('busy') and final.get('indexed', 0) >= 2:
                    break
                time.sleep(.05)
            explicit = req('search', text=TOKEN)
        after_explicit = witness()
        idle_before = req('status')['status']
        time.sleep(10)
        idle_after = req('status')['status']
        worker_pids = sorted(tree(proc.pid))
        shutdown = req('shutdown')
        c.close()
        c = None
        proc.wait(timeout=25)
        for pid in worker_pids:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                continue
            # A PID can be recycled; only count a live process whose original proc entry remains.
            if Path(f'/proc/{pid}').exists():
                raise RuntimeError(f'owned child process remains after shutdown: {pid}')
        return {
            'name': label, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
            'probe_sha256': hashlib.sha256(PROBE.read_bytes()).hexdigest(),
            'before': before, 'creation_clock_ns': [create_before, create_after], 'after': after,
            'pre_scan_status': observed, 'pre_scan_search': query, 'pre_scan_body_positive': pre_scan_positive,
            'explicit_scan_search': explicit, 'after_explicit': after_explicit,
            'idle_before': idle_before, 'idle_after': idle_after, 'child_pids_before_shutdown': worker_pids,
            'shutdown': shutdown, 'closure': {'service_exit': proc.returncode,
                'children_absent': all(not Path(f'/proc/{pid}').exists() for pid in worker_pids),
                'socket_absent': not sockpath.exists()}
        }
    finally:
        if c:
            c.close()
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(timeout=10)
        log.close()


if __name__ == '__main__':
    O.mkdir(parents=True, exist_ok=True)
    results = [
        run_case('original', BIN / 'service-original', 4.0),
        run_case('service-v2', BIN / 'service-v2', 25.0),
    ]
    original, candidate = results
    original_auto = original['pre_scan_status'].get('indexed') == 2 and original['pre_scan_body_positive']
    original_explicit = any(r.get('path', '').endswith('/auto.txt') and BODY in r.get('text', '')
                            for r in original['explicit_scan_search'].get('results', []))
    candidate_auto = candidate['pre_scan_status'].get('indexed') == 2 and candidate['pre_scan_body_positive']
    root_absent_at_creation = not original['before']['root_watch_installed'] and not candidate['before']['root_watch_installed']
    root_present_after = original['after']['root_watch_installed'] and candidate['after']['root_watch_installed']
    passed = root_absent_at_creation and root_present_after and not original_auto and original_explicit and candidate_auto
    payload = {'outcome': 'pass' if passed else 'fail', 'checks': {
        'actual_root_watch_absent_at_creation_both': root_absent_at_creation,
        'actual_root_watch_present_after_both': root_present_after,
        'original_did_not_automatically_index': not original_auto,
        'original_explicit_scan_body_positive': original_explicit,
        'candidate_automatically_indexed_body': candidate_auto,
        'candidate_idle_revision_stable': candidate['idle_before'].get('index_revision') == candidate['idle_after'].get('index_revision'),
        'all_processes_and_sockets_closed': all(r['closure']['service_exit'] == 0 and r['closure']['children_absent'] and r['closure']['socket_absent'] for r in results)
    }, 'cases': results}
    (P / 'evidence/service-watch-v1/results.json').write_text(json.dumps(payload, indent=2) + '\n')
    print(json.dumps({'outcome': payload['outcome'], 'checks': payload['checks']}))
    raise SystemExit(0 if passed else 1)
