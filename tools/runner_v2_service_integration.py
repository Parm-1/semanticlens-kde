#!/usr/bin/env python3
"""Exercise the native runner against an isolated actual CPU service and synthetic root."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

project = Path(__file__).resolve().parents[1]
out = project / "local/runner-v2-service"
if out.exists():
    raise SystemExit(f"preserving existing output: {out}")
runtime = out / "runtime"
corpus = runtime / "corpus"
data = runtime / "data"
corpus.mkdir(parents=True)
data.mkdir()
source = corpus / "runner-result.txt"
source.write_text("RUNNER-SERVICE-MARKER-81273: this exact identifier appears in the synthetic runner integration fixture.\n")
socket_name = f"/tmp/kde-index-runner-v2-{os.getpid()}"
log_file = (out / "service.log").open("wb")
service = None
result = {"corpus": "one synthetic text fixture", "socket": "isolated per-process local socket", "checks": []}

def check(name, ok, detail=None):
    result["checks"].append({"name": name, "pass": bool(ok), "detail": detail})
    if not ok:
        raise RuntimeError(f"{name} failed: {detail}")

def cli(op, **fields):
    request = {"op": op, **fields}
    proc = subprocess.run([str(project / "build-engine/kde-index-cli"), "--socket", socket_name,
                           json.dumps(request, separators=(",", ":"))], capture_output=True, text=True, timeout=35)
    if proc.returncode:
        raise RuntimeError(f"CLI {op} failed: {proc.stderr.strip()}")
    return json.loads(proc.stdout)

try:
    probe = project / "build-probe/embedding-probe"
    service = subprocess.Popen([
        str(project / "build-engine/kde-index-service"), "--data-dir", str(data), "--socket", socket_name,
        "--probe", str(probe), "--text-model", str(project / "models/embeddinggemma-2-Q8_0.gguf"),
        "--image-model", str(project / "models/mmproj-embeddinggemma-2-Q8_0.gguf"), "--threads", "1",
        "--probe-sha256", hashlib.sha256(probe.read_bytes()).hexdigest()
    ], stdout=log_file, stderr=log_file)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        if service.poll() is not None:
            raise RuntimeError(f"service exited {service.returncode}")
        try:
            status = cli("status").get("status", {})
            if status.get("ready"):
                break
        except (OSError, subprocess.SubprocessError, RuntimeError):
            pass
        time.sleep(0.1)
    else:
        raise TimeoutError("service did not become ready")
    check("actual-service-ready", status.get("ready") and status.get("model_ready"), status)
    added = cli("add_root", path=str(corpus))
    check("selected-synthetic-root", added.get("accepted") is True, added)
    scanned = cli("scan")
    check("scan-accepted", scanned.get("accepted") is True, scanned)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        status = cli("status").get("status", {})
        if not status.get("busy"):
            break
        time.sleep(0.1)
    check("synthetic-indexed", status.get("indexed") == 1 and status.get("segments", 0) > 0, status)
    native = subprocess.run([
        str(project / "build-runner-v2-actual-service/runner-v2-actual-service"),
        str(project / "build-runner-v2/libkde-index-runner-v2.so"), str(source), socket_name
    ], capture_output=True, text=True, timeout=15, env={**os.environ,
        "KDE_INDEX_SOCKET": socket_name,
        "XDG_CONFIG_HOME": str(project / "build-runner-v2/xdg/config"),
        "XDG_CACHE_HOME": str(project / "build-runner-v2/xdg/cache")})
    result["runner"] = json.loads(native.stdout) if native.stdout else {"stderr": native.stderr}
    check("native-RunnerManager-actual-service", native.returncode == 0 and result["runner"].get("pass") is True,
          {"exit": native.returncode, "runner": result["runner"], "stderr": native.stderr})
    result["service_identity"] = status.get("identity")
finally:
    if service and service.poll() is None:
        try:
            cli("shutdown")
        except Exception as e:
            result["shutdown_error"] = str(e)
        try:
            service.wait(timeout=10)
        except subprocess.TimeoutExpired:
            service.terminate()
            try:
                service.wait(timeout=5)
            except subprocess.TimeoutExpired:
                service.kill(); service.wait()
    log_file.close()
    result["resource_closure"] = {
        "service_exited": service is None or service.poll() is not None,
        "socket_absent": not Path(socket_name).exists(),
        "service_exit_code": None if service is None else service.returncode,
    }
    def sanitize(value):
        if isinstance(value, str):
            return value.replace(str(corpus), "<synthetic-corpus>")
        if isinstance(value, list):
            return [sanitize(item) for item in value]
        if isinstance(value, dict):
            return {key: sanitize(item) for key, item in value.items()}
        return value
    (project / "evidence/runner-v2/actual-service.json").write_text(json.dumps(sanitize(result), indent=2) + "\n")
print(json.dumps({"checks": len(result["checks"]), "resource_closure": result["resource_closure"]}))
