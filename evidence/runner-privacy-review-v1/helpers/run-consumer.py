from pathlib import Path
import hashlib,json,subprocess,time
root=Path.cwd();o=root/'evidence/runner-privacy-review-v1';binary=root/'local/build-review/runner-privacy-review-v1/consumer';data=root/'evidence/runner-privacy-v1/manager-v5.jsonl';runtime=root/'local/build-review/runner-privacy-review-v1/runtime'
cmd=['bwrap','--ro-bind','/','/','--tmpfs','/home','--tmpfs','/root','--tmpfs','/run','--tmpfs','/tmp','--unshare-pid','--unshare-net','--proc','/proc','--dev','/dev','--ro-bind',str(binary),'/tmp/consumer','--ro-bind',str(data),'/tmp/matches.jsonl','--bind',str(runtime),'/tmp/runtime','--clearenv']
for k,v in {'PATH':'/usr/bin:/bin','HOME':'/tmp','XDG_CONFIG_HOME':'/tmp/config','XDG_CACHE_HOME':'/tmp/cache','XDG_DATA_HOME':'/tmp/data','XDG_RUNTIME_DIR':'/tmp/runtime','QT_QPA_PLATFORM':'offscreen','QT_QUICK_BACKEND':'software','LANG':'C.UTF-8'}.items():cmd+=['--setenv',k,v]
cmd+=['dbus-run-session','--','/tmp/consumer','/tmp/matches.jsonl']
a=time.monotonic_ns();r=subprocess.run(cmd,capture_output=True,timeout=15);b=time.monotonic_ns();(o/'consumer.json').write_bytes(r.stdout);(o/'consumer.stderr').write_bytes(r.stderr)
receipt={'exit':r.returncode,'elapsed_ns':b-a,'clock':'time.monotonic_ns outer wrapper; includes setup/Qt/namespace/closure','binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'input_sha256':hashlib.sha256(data.read_bytes()).hexdigest(),'private_pid_network_namespace':True,'wayland_used':False,'closure':'outer bwrap process returned; PID namespace teardown closes all namespace children'}
(o/'consumer-run.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt));raise SystemExit(r.returncode)
