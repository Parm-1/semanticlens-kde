#!/usr/bin/env python3
"""Run cold/warm/reset KRunner queries against the frozen final service binary."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

project = Path(__file__).resolve().parents[1]
run_id = os.environ.get("RUNNER_V3_RUN_ID", "")
if run_id and not all(c.isalnum() or c in "-_" for c in run_id):
    raise SystemExit("RUNNER_V3_RUN_ID must be a short path-safe token")
out = project / ("local/runner-v3" + ("-" + run_id if run_id else ""))
if out.exists():
    raise SystemExit(f"preserving prior output: {out}")
evidence = project / "evidence/runner-v3"
runtime = out / "runtime"
corpus = runtime / "corpus"
data = runtime / "data"
corpus.mkdir(parents=True)
data.mkdir()
source = corpus / "cold-result.txt"
marker = "RUNNER-V3-COLD-IDENTIFIER-81356"
source.write_text(f"{marker}: exact path through two cold producer reloads and a warm query.\n")
socket_name = str(runtime / "service.sock")
service_bin = project / "evidence/engine-v1/candidate/bin/kde-index-service"
cli_bin = project / "evidence/engine-v1/candidate/bin/kde-index-cli"
probe = project / "build-probe/embedding-probe"
text_model = project / "models/embeddinggemma-2-Q8_0.gguf"
image_model = project / "models/mmproj-embeddinggemma-2-Q8_0.gguf"
module = project / "build-runner-v3/libkde-index-runner.so"
native = project / "build-runner-v3-actual-service/runner-v3-actual-service"
EXPECTED = {
    str(service_bin): "f41633a233a61a4592ae5350b8659401d6fbaf5c8cd4c8d73fffe2c1ed282d12",
    str(probe): "f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf",
    str(text_model): "2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135",
    str(image_model): "c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f",
}

def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()

# Reverify the frozen service delivery pins immediately before invoking it.
subprocess.run(["sha256sum", "-c", "pins.sha256"], cwd=project / "evidence/engine-v1", check=True,
               stdout=(evidence / "service-pins.log").open("wb"))
for path, expected in EXPECTED.items():
    actual = digest(Path(path))
    if actual != expected:
        raise RuntimeError(f"input hash mismatch for {Path(path).name}: {actual}")

record = {"service_sha256": EXPECTED[str(service_bin)], "probe_sha256": EXPECTED[str(probe)],
          "model_sha256": {Path(p).name: h for p, h in EXPECTED.items() if "model" in p},
          "thread_count": 6, "marker": marker, "runs": [], "checks": []}
service_log = (out / "service.log").open("wb")
service = None
known_children = set()
serial = 0

def check(name, passed, detail=None):
    record["checks"].append({"name": name, "pass": bool(passed), "detail": detail})
    if not passed:
        raise RuntimeError(f"{name} failed: {detail}")

def request(op, **fields):
    global serial
    serial += 1
    payload = {"id": str(serial), "op": op, **fields}
    command = [str(cli_bin), "--socket", socket_name, json.dumps(payload, separators=(",", ":"))]
    result = subprocess.run(command, capture_output=True, text=True, timeout=40)
    response = json.loads(result.stdout) if result.stdout.strip() else None
    if result.returncode and not isinstance(response, dict):
        raise RuntimeError(f"frozen CLI {op}: {result.stderr.strip()} {result.stdout.strip()}")
    return {"request": payload, "response": response, "cli_exit": result.returncode}

def status():
    response = request("status")["response"]
    if not response or not response.get("ok"):
        return {"ready": False, "error": response.get("error", "empty status response") if response else "empty status response"}
    return response["status"]

def child_snapshot():
    found = []
    if service is None or not Path(f"/proc/{service.pid}").exists():
        return found
    for task_file in Path(f"/proc/{service.pid}/task").glob("*/children"):
        try:
            pids = map(int, task_file.read_text().split())
        except (FileNotFoundError, ProcessLookupError):
            continue
        for pid in pids:
            try:
                exe = os.readlink(f"/proc/{pid}/exe")
            except FileNotFoundError:
                continue
            found.append({"pid": pid, "exe": exe})
            known_children.add(pid)
    return found

def wait_indexed():
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        snapshot = status()
        if not snapshot.get("ready"):
            raise RuntimeError(f"service lost readiness: {snapshot}")
        if not snapshot.get("busy") and snapshot.get("indexed") == 1 and snapshot.get("segments") == 1:
            return snapshot
        time.sleep(0.1)
    raise TimeoutError("synthetic fixture did not index")

def wait_producer_unloaded(label):
    before = child_snapshot()
    started = time.monotonic()
    deadline = started + 24
    after = before
    while time.monotonic() < deadline:
        after = child_snapshot()
        if not after:
            break
        time.sleep(0.1)
    if after:
        raise TimeoutError(f"producer remained active before {label}: {after}")
    elapsed_ms = round((time.monotonic() - started) * 1000, 3)
    check(f"{label}-observed-producer-unload", bool(before) and not after and elapsed_ms >= 9000,
          {"before": before, "after": after, "idle_ms": elapsed_ms})
    return {"label": label, "before_children": before, "after_children": after, "observed_idle_ms": elapsed_ms,
            "status": status()}

def native_query(label, reset_ms=0):
    environment = {**os.environ,
                   "KDE_INDEX_SOCKET": socket_name,
                   "XDG_CONFIG_HOME": str(project / "build-runner-v3/xdg/config"),
                   "XDG_DATA_HOME": str(project / "build-runner-v3/xdg/data"),
                   "XDG_CACHE_HOME": str(project / "build-runner-v3/xdg/cache"),
                   "XDG_RUNTIME_DIR": str(project / "build-runner-v3/xdg/runtime")}
    runtime_dir = project / "build-runner-v3/xdg/runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    runtime_dir.chmod(0o700)
    started = time.monotonic_ns()
    native_result = subprocess.run([str(native), str(module), socket_name, f"find {marker}", str(source), str(reset_ms)],
                                   capture_output=True, text=True, timeout=16, env=environment)
    elapsed_ms = round((time.monotonic_ns() - started) / 1e6, 3)
    parsed = json.loads(native_result.stdout) if native_result.stdout else {"stderr": native_result.stderr}
    observation = {"label": label, "process_elapsed_ms": elapsed_ms, "exit_code": native_result.returncode,
                   "result": parsed, "stderr": native_result.stderr, "children_after": child_snapshot()}
    record["runs"].append(observation)
    if native_result.returncode or not parsed.get("pass"):
        raise RuntimeError(f"native run {label} failed: {observation}")
    if reset_ms:
        check(f"{label}-reset-no-stale-results", parsed.get("reset_observed") and not parsed.get("matches")
              and 100 <= parsed.get("reset_to_final_observation_ms", 0) <= 500,
              {"runner": parsed, "children_after": observation["children_after"]})
    else:
        match = parsed["matches"][0]
        check(f"{label}-exact-hit", match["data"]["path"] == str(source) and match["relevance"] == 1,
              {"runner": parsed})
    return observation

try:
    for path, expected in EXPECTED.items():
        check(f"pinned-input-{Path(path).name}", digest(Path(path)) == expected, {"expected": expected})
    service = subprocess.Popen([str(service_bin), "--data-dir", str(data), "--socket", socket_name,
                                "--probe", str(probe), "--text-model", str(text_model), "--image-model", str(image_model),
                                "--threads", "6", "--probe-sha256", EXPECTED[str(probe)]],
                               stdout=service_log, stderr=service_log)
    record["service_pid"] = service.pid
    deadline = time.monotonic() + 35
    ready = None
    startup_errors = []
    while time.monotonic() < deadline:
        if service.poll() is not None:
            raise RuntimeError(f"frozen service exited early with {service.returncode}")
        try:
            ready = status()
            if ready.get("error"):
                raise RuntimeError(f"service startup error: {ready['error']}")
            if ready.get("ready"):
                break
        except (OSError, subprocess.SubprocessError, RuntimeError) as error:
            startup_errors.append(str(error))
            time.sleep(0.1)
    if not ready or not ready.get("ready") or not ready.get("model_ready"):
        raise TimeoutError(f"frozen service did not become ready: {ready}; log={service_log.name}")
    record["startup_retry_count"] = len(startup_errors)
    record["service_identity"] = ready.get("identity")
    record["initial_status"] = ready
    check("service-producer-identity", ready["identity"].get("producer_sha256") == EXPECTED[str(probe)], ready["identity"])
    added = request("add_root", path=str(corpus))
    check("selected-synthetic-root", added["response"].get("accepted") is True, added)
    scan = request("scan")
    check("scan-accepted", scan["response"].get("accepted") is True, scan)
    indexed = wait_indexed()
    check("one-synthetic-file-indexed", indexed.get("indexed") == 1 and indexed.get("segments") == 1 and indexed.get("errors") == 0, indexed)
    record["indexed_status"] = indexed

    # Warm result first; then observe producer unload via /proc rather than assuming sleep implies cold.
    native_query("warm-before-first-idle")
    record["idle_before_cold_1"] = wait_producer_unloaded("idle-before-cold-1")
    native_query("cold-query-1")
    native_query("warm-after-cold-1")
    record["idle_before_cold_2"] = wait_producer_unloaded("idle-before-cold-2")
    native_query("cold-query-2")
    native_query("warm-after-cold-2")
    record["idle_before_cold_reset"] = wait_producer_unloaded("idle-before-cold-reset")
    native_query("cold-reset", reset_ms=105)
    record["final_status"] = status()
except Exception as error:
    record["failure"] = repr(error)
    record["service_log_tail"] = "see local runner-v3 service.log"
    raise
finally:
    if service and service.poll() is None:
        try:
            record["shutdown"] = request("shutdown")
        except Exception as error:
            record["shutdown_error"] = str(error)
        try:
            service.wait(timeout=15)
        except subprocess.TimeoutExpired:
            service.terminate()
            try:
                service.wait(timeout=5)
            except subprocess.TimeoutExpired:
                service.kill(); service.wait()
    service_log.close()
    deadline = time.monotonic() + 5
    remaining_children = child_snapshot()
    while remaining_children and time.monotonic() < deadline:
        time.sleep(0.1)
        remaining_children = child_snapshot()
    record["resource_closure"] = {
        "service_exit_code": None if service is None else service.returncode,
        "service_pid_absent": service is None or not Path(f"/proc/{service.pid}").exists(),
        "socket_absent": not Path(socket_name).exists(),
        "known_probe_children_absent": not remaining_children,
        "known_child_pids": sorted(known_children),
        "remaining_children": remaining_children,
    }
    def sanitize(value):
        if isinstance(value, str):
            return value.replace(str(project), "<workspace>").replace(str(runtime), "<runner-v3-runtime>")
        if isinstance(value, list):
            return [sanitize(item) for item in value]
        if isinstance(value, dict):
            return {key: sanitize(item) for key, item in value.items()}
        return value
    (evidence / "actual-cold-service.json").write_text(json.dumps(sanitize(record), indent=2) + "\n")
print(json.dumps({"cold_runs": 2, "runs": len(record["runs"]), "closure": record["resource_closure"]}))
