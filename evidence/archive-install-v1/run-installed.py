import json,os,subprocess
from pathlib import Path
root=Path('local/clean-arch-v1/root.x86_64').resolve(); base=root/'archive-source-v1'
src=base/'kde-index-0.1.0-dev-archivecheck-source'
helper=base/'default-runner-src';helper.mkdir(exist_ok=False)
original=(src/'tools/installed_runner.cpp').read_text()
assert original.count('QCoreApplication::addLibraryPath(root);')==1
modified=original.replace('QCoreApplication::addLibraryPath(root);','// Standard /usr plugin discovery; no library path injection.')
modified=modified.replace('{{"discovered", discovered}, {"finished", finished}', '{{"library_paths", QJsonArray::fromStringList(QCoreApplication::libraryPaths())}, {"discovered", discovered}, {"finished", finished}')
(helper/'installed_runner.cpp').write_text(modified)
cm=(src/'tools/installed_runner/CMakeLists.txt').read_text().replace('../installed_runner.cpp','installed_runner.cpp');(helper/'CMakeLists.txt').write_text(cm)
output=base/'installed-target-v1';runtime=base/'native-runtime';runtime.mkdir(exist_ok=False);runtime.chmod(0o700)
wayland=Path(os.environ['XDG_RUNTIME_DIR'])/os.environ['WAYLAND_DISPLAY'];assert wayland.is_socket()
cmd=['bwrap','--bind',str(root),'/','--unshare-user','--uid','0','--gid','0','--unshare-pid','--unshare-ipc','--unshare-uts','--unshare-cgroup','--unshare-net','--die-with-parent','--dev','/dev','--proc','/proc','--tmpfs','/tmp','--ro-bind',str(Path('models').resolve()),'/models','--ro-bind',str(wayland),'/archive-source-v1/native-runtime/wayland-0','--clearenv']
env={'PATH':'/usr/bin','HOME':'/tmp/home','XDG_RUNTIME_DIR':'/archive-source-v1/native-runtime','WAYLAND_DISPLAY':'wayland-0','QT_QPA_PLATFORM':'wayland','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8','XDG_DATA_DIRS':'/usr/share'}
for k,v in env.items():cmd+=['--setenv',k,v]
cmd+=['--chdir','/archive-source-v1','/usr/bin/python3','-c',"import subprocess; subprocess.run(['/usr/bin/cmake','-S','default-runner-src','-B','default-runner-build','-G','Ninja'],check=True); subprocess.run(['/usr/bin/cmake','--build','default-runner-build','-j','2'],check=True); subprocess.run(['/usr/bin/python3','kde-index-0.1.0-dev-archivecheck-source/tools/installed_integration.py','--stage','/usr','--models','/models','--runner-check','/archive-source-v1/default-runner-build/installed-runner','--output','/archive-source-v1/installed-target-v1'],check=True)"]
Path('local/archive-install-v1/native-command.json').write_text(json.dumps({'command':cmd,'scope':'Actual /usr install in own Arch userspace; standard Qt library paths and host Wayland socket, private config/data, no host install/network'},indent=2)+'\n')
with Path('local/archive-install-v1/installed-target.log').open('wb') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=150)
print(json.dumps({'exit_code':r.returncode}));raise SystemExit(r.returncode)
