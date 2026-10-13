#!/usr/bin/env python3
"""Preserve a real rc2 multimodal index across S1 imports and native runner queries."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import sqlite3
import subprocess
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'service', 'service-sha256', 'tokenizer', 'tokenizer-sha256'):
        p.add_argument('--'+name, required=True)
    p.add_argument('--project-root', default=str(Path(__file__).resolve().parents[1]))
    p.add_argument('--prepare-only', action='store_true')
    p.add_argument('--film-worker')
    p.add_argument('--film-worker-sha256')
    a = p.parse_args()
    if bool(a.film_worker) != bool(a.film_worker_sha256):
        p.error('--film-worker and --film-worker-sha256 must be supplied together')
    root, out = Path(a.project_root).resolve(), Path(a.output).resolve()
    out.mkdir(parents=True, exist_ok=False)
    service, tokenizer = Path(a.service).resolve(), Path(a.tokenizer).resolve()
    stage = root/'local/release-rc2-stage-v1/usr'
    baseline, probe = stage/'bin/kde-index-service', stage/'bin/embedding-probe'
    pluginroot = stage/'lib/qt6/plugins'
    helper = root/'build-runner-discovery-v1-helper/installed-runner-v2'
    models = root/'models'
    pins = {service: a.service_sha256, tokenizer: a.tokenizer_sha256,
            baseline: '28c0d5ddf0ab20608ef527dd891cb023ecd2194690b9f281b95533dacd202ebe',
            probe: 'fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0',
            pluginroot/'kf6/krunner/kde-index-runner.so': 'a587a20400e4892c0eded5af30c08e646ea09885a971eb3a6a6e6c61bda2e9ba',
            helper: '6331f961d570f0a607eaf600465970aea86c5285f3bb9c4ebe9d9c4c980ff0b5',
            models/'embeddinggemma-2-Q8_0.gguf': '2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135',
            models/'mmproj-embeddinggemma-2-Q8_0.gguf': 'c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f'}
    film_worker = Path(a.film_worker).resolve() if a.film_worker else None
    if film_worker:
        pins[film_worker] = a.film_worker_sha256
    for path, expected in pins.items():
        if sha(path) != expected:
            raise ValueError('Pinned input mismatch: '+str(path))
    corpus = out/'legacy-corpus'
    corpus.mkdir()
    for name in ('payment.txt', 'manual.pdf', 'picture.jpg'):
        shutil.copy2(root/'evidence/combined-native-v1/inputs'/name, corpus/name)
    shutil.copy2(root/'benchmark/corpus/i05.png', corpus/'i05.png')
    film = out/'film-corpus'
    shutil.copytree(root/'local/film-fixtures-v1/corpus', film)
    data = out/'data'
    report = {'scope': 'actual rc2 index reuse, unchanged legacy vectors, file/PDF/image/OCR and native RunnerManager compatibility',
              'pins': {str(path): digest for path, digest in pins.items()},
              'input_pins': {str(path): sha(path) for directory in (corpus, film) for path in sorted(directory.rglob('*')) if path.is_file()},
              'checks': [], 'sessions': [], 'native': [], 'records': [], 'passed': None}
    if a.prepare_only:
        report['status'] = 'inputs_prepared_runtime_not_run'
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({'status': report['status']}))
        return 0
    current = None

    def check(name, value, detail=None):
        report['checks'].append({'name': name, 'passed': bool(value), 'detail': detail})
        if not value:
            raise AssertionError(name)

    class Client:
        def __init__(self, sock):
            self.socket = socket.socket(socket.AF_UNIX)
            self.socket.settimeout(45)
            self.socket.connect(str(sock))
            self.buf, self.seq = b'', 0

        def call(self, op, **fields):
            self.seq += 1
            request = {'id': 'legacy-'+str(id(self))+'-'+str(self.seq), 'op': op, **fields}
            t0 = time.monotonic_ns()
            self.socket.sendall((json.dumps(request, ensure_ascii=False)+'\n').encode())
            while b'\n' not in self.buf:
                chunk = self.socket.recv(1024*1024)
                if not chunk:
                    raise RuntimeError('Service EOF')
                self.buf += chunk
            line, self.buf = self.buf.split(b'\n', 1)
            reply = json.loads(line)
            report['records'].append({'request': request, 'reply': reply, 'start_ns': t0, 'end_ns': time.monotonic_ns()})
            if reply.get('id') != request['id'] or not reply.get('ok'):
                raise RuntimeError(repr(reply))
            return reply

    class Session:
        def __init__(self, binary, name, film_enabled):
            self.client = None
            self.sock = out/(name+'.sock')
            if len(os.fsencode(self.sock)) > 100:
                raise ValueError('Choose a shorter absolute output path')
            self.log = (out/(name+'.log')).open('wb')
            argv = [str(binary), '--data-dir', str(data), '--socket', str(self.sock), '--probe', str(probe),
                    '--probe-sha256', pins[probe], '--text-model', str(models/'embeddinggemma-2-Q8_0.gguf'),
                    '--image-model', str(models/'mmproj-embeddinggemma-2-Q8_0.gguf'), '--threads', '2']
            if film_enabled:
                argv += ['--film-tokenizer', str(tokenizer), '--film-tokenizer-sha256', pins[tokenizer]]
                if film_worker:
                    argv += ['--film-worker', str(film_worker), '--film-worker-sha256', pins[film_worker]]
            self.receipt = {'name': name, 'argv': argv, 'sha256_at_launch': sha(binary)}
            report['sessions'].append(self.receipt)
            self.proc = subprocess.Popen(argv, stdout=self.log, stderr=self.log, start_new_session=True)
            self.receipt['pid'] = self.proc.pid

        def connect(self):
            deadline = time.monotonic()+40
            while time.monotonic() < deadline:
                try:
                    self.client = Client(self.sock)
                    return
                except (FileNotFoundError, ConnectionRefusedError):
                    if self.proc.poll() is not None:
                        raise RuntimeError('Early service exit')
                    time.sleep(.05)
            raise TimeoutError('Private service socket')

        def close(self):
            if self.proc.poll() is None:
                try:
                    if self.client:
                        self.client.call('shutdown')
                    self.proc.wait(timeout=15)
                except Exception:
                    os.killpg(self.proc.pid, signal.SIGTERM)
                    try:
                        self.proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        os.killpg(self.proc.pid, signal.SIGKILL)
                        self.proc.wait(timeout=10)
            if self.client:
                self.client.socket.close()
            self.log.close()
            remaining = []
            for path in Path('/proc').iterdir():
                if not path.name.isdecimal():
                    continue
                try:
                    raw = (path/'stat').read_text()
                    if int(raw[raw.rfind(')')+2:].split()[2]) == self.proc.pid:
                        remaining.append(int(path.name))
                except (FileNotFoundError, ProcessLookupError, PermissionError):
                    pass
            self.receipt['closure'] = {'exit': self.proc.returncode, 'socket_absent': not self.sock.exists(), 'remaining_group': remaining}

    def wait_status(predicate, seconds=120):
        deadline = time.monotonic()+seconds
        while time.monotonic() < deadline:
            status = current.client.call('status')['status']
            if predicate(status):
                return status
            if status.get('error'):
                raise RuntimeError(status['error'])
            time.sleep(.04)
        raise TimeoutError('Legacy status condition: '+repr(status))

    def rows():
        with sqlite3.connect('file:'+str(data/'index.sqlite')+'?mode=ro', uri=True) as db:
            identity, sql_type = db.execute("select value,typeof(value) from meta where key='identity'").fetchone()
            identity_bytes = identity.encode('utf-8') if isinstance(identity, str) else bytes(identity)
            if isinstance(identity, bytes):
                identity = identity.decode('utf-8')
            return {'identity': identity, 'identity_sql_type': sql_type,
                    'identity_raw_base64': base64.b64encode(identity_bytes).decode('ascii'),
                    'vectors': [dict(id=row[0], path=row[1], page=row[2], kind=row[3], sha256=hashlib.sha256(row[4]).hexdigest(),
                                     raw_base64=base64.b64encode(row[4]).decode('ascii'))
                                                      for row in db.execute('select id,path,page,kind,vector from segments order by id')]}

    def file_queries(label):
        for text, filename, kind, page in [('CARD-39312', 'payment.txt', 'text', 0),
                                            ('PDF-10299', 'manual.pdf', 'pdf', 2),
                                            ('BAK-84319', 'i05.png', 'image', 0)]:
            reply = current.client.call('search', text=text, limit=12)
            check(label+' '+text, any(row['path']==str(corpus/filename) and row['kind']==kind and row['page']==page and row['exact']
                                     for row in reply['results']), reply['results'])
        reply = current.client.call('search', text='picture', limit=12)
        check(label+' image vector exists', any(row['path']==str(corpus/'picture.jpg') and row['kind']=='image' for row in reply['results']))

    def await_job(jid):
        deadline = time.monotonic()+120
        while time.monotonic() < deadline:
            job = current.client.call('film_status', job_id=jid)['status']['jobs'][0]
            if job['state'] in ('complete', 'failed', 'canceled', 'interrupted'):
                check('film import completed '+jid, job['state']=='complete', job)
                return job
            time.sleep(.02)
        raise TimeoutError('Film job')

    def import_file(op, relative):
        digest = current.client.call('film_sidecar_digest', collection_id=cid, relative_path=relative)['sha256']
        return current.client.call(op, collection_id=cid, relative_path=relative, expected_sha256=digest)['job_id']

    try:
        current = Session(baseline, 'rc2', False)
        current.connect()
        status = wait_status(lambda status: status['ready'])
        check('rc2 accepted producer', status['identity']['producer_sha256']==pins[probe])
        current.client.call('add_root', path=str(corpus))
        wait_status(lambda status: not status['busy'] and status['indexed']==4)
        file_queries('rc2')
        old = rows()
        report['rc2_index'] = old
        current.close()
        current = Session(service, 's1', True)
        current.connect()
        wait_status(lambda status: status['ready'] and not status['busy'])
        report['s1_reopened_index'] = rows()
        check('existing rc2 identity and vector bytes retained', report['s1_reopened_index']==old, report['s1_reopened_index'])
        file_queries('S1 before film import')
        cid = current.client.call('film_add_collection', path=str(film))['collection_id']
        for name in ('harbor', 'orchard'):
            await_job(import_file('film_import_manifest', name+'/manifest.json'))
        await_job(import_file('film_import_analysis', 'harbor/analysis.json'))
        report['s1_after_film_imports_index'] = rows()
        check('film ingestion preserves existing vectors', report['s1_after_film_imports_index']==old)
        file_queries('S1 after film import')
        # RunnerManager uses QCoreApplication, not a graphical frontend. Run its
        # backend compatibility control in a private session with no display,
        # clipboard, host session bus or access to personal homes.
        for name in ('config', 'cache', 'data-home', 'home', 'runtime'):
            (out/name).mkdir(mode=0o700)
        command = ['bwrap', '--unshare-user', '--unshare-pid', '--unshare-net',
                   '--die-with-parent', '--ro-bind', '/', '/', '--tmpfs', '/home',
                   '--tmpfs', '/run/user', '--tmpfs', '/tmp', '--bind', str(out), '/tmp/test',
                   '--ro-bind', str(helper), '/tmp/test/runner',
                   '--ro-bind', str(pluginroot), '/tmp/test/plugins',
                   '--dev', '/dev', '--proc', '/proc', '--clearenv',
                   '--setenv', 'PATH', '/usr/bin', '--setenv', 'LANG', 'C.UTF-8',
                   '--setenv', 'HOME', '/tmp/test/home',
                   '--setenv', 'XDG_RUNTIME_DIR', '/tmp/test/runtime',
                   '--setenv', 'XDG_CONFIG_HOME', '/tmp/test/config',
                   '--setenv', 'XDG_CACHE_HOME', '/tmp/test/cache',
                   '--setenv', 'XDG_DATA_HOME', '/tmp/test/data-home',
                   'dbus-run-session', '--', '/tmp/test/runner', '/tmp/test/plugins',
                   '/tmp/test/runner.ini', '/tmp/test/'+current.sock.name,
                   'find CARD-39312', str(corpus/'payment.txt')]
        start = time.monotonic_ns()
        runner = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        other = Client(current.sock)
        try:
            reply = other.call('film_search', text='CLOCK-7320', mode='source', collection_id=cid)
            check('separate film client literal', reply['results'][0]['source_exact'] and reply['results'][0]['film_id']=='film:orchard')
            stdout, stderr = runner.communicate(timeout=20)
        finally:
            other.socket.close()
            if runner.poll() is None:
                os.killpg(runner.pid, signal.SIGTERM)
                try:
                    runner.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(runner.pid, signal.SIGKILL)
                    runner.wait(timeout=5)
        native = json.loads(stdout)
        report['native'].append({'argv': command, 'begin_ns': start, 'end_ns': time.monotonic_ns(),
                                 'exit': runner.returncode, 'stdout': native, 'stderr': stderr.decode(errors='replace'),
                                 'scope': 'non-graphical RunnerManager backend in a private display-less D-Bus/namespace session'})
        check('native installed runner identity/path/off-main-thread', runner.returncode==0 and native['pass'])
        check('shared service survives native/film clients', current.proc.poll() is None)
        report['s1_after_clients_index'] = rows()
        check('legacy vectors intact after clients', report['s1_after_clients_index']==old)
        current.close()
    except Exception as error:
        report['failure'] = repr(error)
    finally:
        if current and not current.log.closed:
            current.close()
        report['passed'] = not report.get('failure') and all(row['passed'] for row in report['checks']) and all(
            row.get('closure', {}).get('socket_absent') and not row.get('closure', {}).get('remaining_group', [1]) for row in report['sessions'])
        (out/'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'passed': report['passed'], 'checks': len(report['checks']), 'failure': report.get('failure')}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
