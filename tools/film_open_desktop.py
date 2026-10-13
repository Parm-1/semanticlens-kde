"""Dedicated KWin command/environment construction; importing this module launches nothing."""
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import stat
import subprocess
import time
import re

CHILD_KEYS = ('WAYLAND_DISPLAY','DISPLAY','DBUS_SESSION_BUS_ADDRESS','XDG_RUNTIME_DIR','HOME',
              'XDG_CONFIG_HOME','XDG_CACHE_HOME','XDG_DATA_HOME','XDG_STATE_HOME',
              'XDG_SESSION_TYPE','XDG_CURRENT_DESKTOP','QT_QPA_PLATFORM')
PRIVATE_ENV = {
    'HOME':'/tmp/test/home','XDG_CONFIG_HOME':'/tmp/test/config',
    'XDG_CACHE_HOME':'/tmp/test/cache','XDG_DATA_HOME':'/tmp/test/data',
    'XDG_STATE_HOME':'/tmp/test/state','XDG_RUNTIME_DIR':'/tmp/test/runtime',
    'XDG_SESSION_TYPE':'wayland','XDG_CURRENT_DESKTOP':'KDE','QT_QPA_PLATFORM':'wayland',
}

def validate_film_worker(worker, worker_sha256):
    if bool(worker)!=bool(worker_sha256):raise ValueError('film worker path and SHA256 must be supplied together')
    if not worker:return None
    worker=Path(worker).resolve(strict=True)
    if not worker.is_file() or not re.fullmatch(r'[0-9a-f]{64}',worker_sha256):
        raise ValueError('film worker must be a file and its SHA256 must be lowercase hexadecimal')
    if hashlib.sha256(worker.read_bytes()).hexdigest()!=worker_sha256:
        raise ValueError('film worker SHA256 mismatch before launch')
    return worker

def film_worker_flags(worker, worker_sha256):
    return ['--film-worker',str(worker),'--film-worker-sha256',worker_sha256] if worker else []

def prepare_worker_mountpoint(output, worker):
    if not worker:return None
    target=Path(output)/'film-worker'
    target.touch(exist_ok=True);target.chmod(0o500)
    return target

def build_command(output, stage, models, worker=None):
    """Pure command construction: no host DISPLAY/Wayland env/path is read or bound."""
    command=['bwrap','--unshare-user','--unshare-pid','--unshare-net','--die-with-parent',
             '--ro-bind','/','/','--tmpfs','/home','--tmpfs','/root','--tmpfs','/run/user','--tmpfs','/tmp',
             '--bind',str(output),'/tmp/test','--ro-bind',str(stage),'/tmp/test/stage',
             '--ro-bind',str(models),'/tmp/test/models','--dev','/dev','--proc','/proc','--clearenv']
    if worker:
        at=command.index('--dev')
        command[at:at]=['--ro-bind',str(worker),'/tmp/test/film-worker']
    env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8',**PRIVATE_ENV,
         'XDG_DATA_DIRS':'/tmp/test/stage/share:/usr/share',
         'QT_PLUGIN_PATH':'/tmp/test/stage/lib/qt6/plugins:/usr/lib/qt6/plugins',
         'QT_QUICK_BACKEND':'software','LIBGL_ALWAYS_SOFTWARE':'1','DESKTOP_SESSION':'plasma',
         'G_MESSAGES_DEBUG':'xdg-desktop-portal'}
    for key,value in env.items():command+=['--setenv',key,value]
    command+=['dbus-run-session','--','kwin_wayland','--virtual','--width','1440','--height','1000',
              '--socket','semanticlens-test','--no-lockscreen','--no-global-shortcuts','--no-kactivities',
              '--exit-with-session','/tmp/test/session.sh']
    return command

def write_session_launcher(output, worker=None, worker_sha256=None):
    script=Path(output)/'session.sh'
    session_command='/usr/bin/python3 /tmp/test/work/run.py --package /tmp/test/work --session'
    if worker:
        session_command+=' --film-worker /tmp/test/film-worker --film-worker-sha256 '+worker_sha256
    script.write_text('''#!/usr/bin/bash
set -eu
# KWin supplies its separate display to this child. This is the private D-Bus.
export LD_PRELOAD=/tmp/test/work/receiver-observer.so
export DESKTOP_OPEN_V3_OBSERVER_ROOT=/tmp/test/work/receivers
export FILM_PDF_OBSERVER_ROOT=/tmp/test/work/receivers
/usr/bin/dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_RUNTIME_DIR HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME XDG_STATE_HOME LD_PRELOAD DESKTOP_OPEN_V3_OBSERVER_ROOT FILM_PDF_OBSERVER_ROOT
exec '''+session_command+'''
''')
    script.chmod(0o700)
    return script

def validate_child_environment():
    """Run in KWin's child before service, model, portal or test-app launch."""
    actual={key:os.environ.get(key) for key in CHILD_KEYS}
    if any(actual.get(key)!=value for key,value in PRIVATE_ENV.items()):
        raise RuntimeError('private child environment mismatch: '+json.dumps(actual))
    if actual['WAYLAND_DISPLAY']!='semanticlens-test' or actual['DISPLAY'] is not None:
        raise RuntimeError('dedicated display required; no host fallback')
    if not str(actual['DBUS_SESSION_BUS_ADDRESS']).startswith('unix:path=/tmp/dbus-'):
        raise RuntimeError('dedicated private session bus required')
    display=Path(PRIVATE_ENV['XDG_RUNTIME_DIR'])/'semanticlens-test'
    if not stat.S_ISSOCK(display.lstat().st_mode) or display.parent.resolve()!=Path('/tmp/test/runtime'):
        raise RuntimeError('dedicated compositor socket not present')
    if any(Path('/home').iterdir()) or list(Path('/run/user').glob('*')):
        raise RuntimeError('personal home/session resources visible')
    if Path('/dev/input').exists() or Path('/dev/dri').exists():
        raise RuntimeError('host input/GPU devices visible')
    actual['dedicated_socket_path']=str(display)
    actual['dedicated_socket_inode']=display.stat().st_ino
    actual['dedicated_socket_observed']=True
    return actual

def process_group(pgid):
    found=[]
    for p in Path('/proc').iterdir():
        if not p.name.isdigit():continue
        try:
            fields=(p/'stat').read_text().rsplit(')',1)[1].split()
            if int(fields[2])==pgid:found.append({'pid':int(p.name),'exe':os.readlink(p/'exe')})
        except (OSError,ValueError,IndexError):pass
    return found

def launch_dedicated(output,stage,models,bound=180,worker=None,worker_sha256=None):
    """Runtime call only after the parent grant; close the test-owned process group."""
    output=Path(output)
    worker=validate_film_worker(worker,worker_sha256)
    prepare_worker_mountpoint(output,worker)
    write_session_launcher(output,worker,worker_sha256);command=build_command(output,stage,models,worker)
    (output/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    binary_pins={path:hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in ['/usr/bin/bwrap','/usr/bin/kwin_wayland','/usr/bin/dbus-run-session','/usr/bin/dbus-update-activation-environment']}
    started=time.monotonic();timed_out=False
    log=(output/'compositor.log').open('wb')
    proc=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,
                          start_new_session=True,env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8'})
    try:proc.wait(timeout=bound)
    except subprocess.TimeoutExpired:
        timed_out=True;os.killpg(proc.pid,signal.SIGTERM)
        try:proc.wait(timeout=5)
        except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait(timeout=5)
    finally:log.close()
    def read_json(path):
        try:return json.loads(path.read_text())
        except (OSError,json.JSONDecodeError):return {}
    session=read_json(output/'session-environment.json');inner=read_json(output/'work/results.json')
    group=process_group(proc.pid);socket_removed=not (output/'runtime/semanticlens-test').exists()
    dedicated=(session.get('WAYLAND_DISPLAY')=='semanticlens-test' and session.get('DISPLAY') is None
               and all(session.get(k)==v for k,v in PRIVATE_ENV.items())
               and str(session.get('DBUS_SESSION_BUS_ADDRESS','')).startswith('unix:path=/tmp/dbus-')
               and session.get('dedicated_socket_observed') is True)
    closed=proc.poll() is not None and not group and socket_removed
    result={'compositor_runtime_binary_pins':binary_pins,'namespace_exit':proc.returncode,'timed_out':timed_out,'hard_bound_seconds':bound,
            'diagnostic_elapsed_seconds':time.monotonic()-started,'process_group_after':group,
        'film_worker':({'source_path':str(worker),'sha256':worker_sha256,
                        'read_only_in_session_path':'/tmp/test/film-worker'} if worker else None),
            'dedicated_socket_removed':socket_removed,'dedicated_environment_valid':dedicated,
            'session_environment':session,'closed':closed,'inner_passed':inner.get('passed'),
            'passed':proc.returncode==0 and not timed_out and dedicated and closed and inner.get('passed') is True,
            'scope':'dedicated virtual KWin plus private D-Bus/profile/data/runtime; no host display binding',
            'package_usage':'unknown; elapsed is harness wall time only'}
    (output/'outer.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
