import json,os,subprocess,shutil
from pathlib import Path
P=Path.cwd();root=(P/'local/clean-arch-v1/root.x86_64').resolve();base=root/'public-install-v2'
helper=base/'default-runner-src';shutil.copytree(P/'evidence/archive-install-v1/default-runner',helper)
runtime=base/'native-runtime';runtime.mkdir();runtime.chmod(0o700)
wayland=Path(os.environ['XDG_RUNTIME_DIR'])/os.environ['WAYLAND_DISPLAY'];assert wayland.is_socket()
cmd=['bwrap','--die-with-parent','--unshare-all','--ro-bind',str(root),'/','--bind',str(base),'/public-install-v2','--dev','/dev','--proc','/proc','--tmpfs','/tmp','--ro-bind',str(P/'models'),'/public-install-v2/models','--ro-bind',str(wayland),'/public-install-v2/native-runtime/wayland-0','--clearenv']
env={'PATH':'/usr/bin','HOME':'/tmp/home','XDG_RUNTIME_DIR':'/public-install-v2/native-runtime','WAYLAND_DISPLAY':'wayland-0','QT_QPA_PLATFORM':'wayland','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8','XDG_DATA_DIRS':'/usr/share'}
for k,v in env.items():cmd+=['--setenv',k,v]
script="import subprocess; subprocess.run(['/usr/bin/cmake','-S','default-runner-src','-B','default-runner-build','-G','Ninja'],check=True); subprocess.run(['/usr/bin/cmake','--build','default-runner-build','-j','2'],check=True); subprocess.run(['/usr/bin/python3','/public-clean-v2/source/kde-index-source/tools/installed_integration.py','--stage','/usr','--models','/public-install-v2/models','--runner-check','/public-install-v2/default-runner-build/installed-runner','--output','/public-install-v2/installed-target'],check=True)"
cmd+=['--chdir','/public-install-v2','/usr/bin/python3','-c',script]
(base/'native-command.json').write_text(json.dumps({'command':cmd,'scope':'Actual owned /usr installation, standard Qt plugin paths, existing host Wayland socket, private data/config, no host installation/network'},indent=2)+'\n')
with (base/'installed-target.log').open('wb') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=170)
print('installed target exit',r.returncode);raise SystemExit(r.returncode)
