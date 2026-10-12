#!/usr/bin/env python3
"""Run the frozen retrieval queries in an isolated Baloo index, never the desktop index."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from baloo_baseline import run, query
from retrieval_service import descendants

ROOT = Path(__file__).resolve().parents[1]


def session(sandbox):
    corpus = sandbox / "home/corpus"
    files = sorted(corpus.iterdir())
    required = [f for f in files if f.suffix in (".txt", ".pdf")]
    report = {"scope": "frozen authored synthetic corpus v1, native queries as typed",
              "version": run(["balooctl6", "--version"]).stdout.strip(), "queries": []}
    daemon = None
    stop_sample = threading.Event()
    sampler = None
    report['sampled_peak_rss_kib'] = 0
    report['rss_scope'] = '50ms /proc sum of daemon/live-descendant resident KiB; shared pages counted per process; sampled peak'

    def sample():
        while not stop_sample.is_set():
            total = 0
            for pid in descendants(daemon.pid):
                try:
                    for line in Path(f'/proc/{pid}/status').read_text().splitlines():
                        if line.startswith('VmRSS:'):
                            total += int(line.split()[1])
                except FileNotFoundError:
                    pass
            report['sampled_peak_rss_kib'] = max(report['sampled_peak_rss_kib'], total)
            stop_sample.wait(.05)
    with (sandbox / "daemon.log").open("wb") as log:
        try:
            indexing_begin = time.monotonic_ns()
            daemon = subprocess.Popen(["/usr/lib/kf6/baloo_file"], stdout=log, stderr=log, start_new_session=True)
            sampler = threading.Thread(target=sample, daemon=True)
            sampler.start()
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                if daemon.poll() is not None:
                    raise RuntimeError("Baloo daemon exited before completion")
                status = run(["balooctl6", "--format", "json", "status", *map(str, files)], check=False)
                if status.returncode == 0:
                    states = json.loads(status.stdout)
                    by_file = {s["file"]: s for s in states}
                    if all(by_file.get(str(f), {}).get("status") == "done" and
                           by_file.get(str(f), {}).get("indexing") == "content" for f in required):
                        report["content_status"] = [{**s, "file": str(Path(s["file"]).relative_to(corpus))} for s in states]
                        indexing_end = time.monotonic_ns()
                        report['indexing'] = {'begin_ns': indexing_begin, 'end_ns': indexing_end,
                                              'elapsed_seconds': (indexing_end - indexing_begin) / 1e9}
                        break
                time.sleep(0.25)
            else:
                raise TimeoutError("Text/PDF content indexing did not finish within 90 seconds")
            report["extracted_terms"] = {f.name: run(["balooshow6", "-x", str(f)], check=False).stdout.replace(str(corpus), "<corpus>") for f in files}
            specification = json.loads((sandbox / "queries.json").read_text())
            for entry in specification["queries"]:
                row = query(corpus, entry["text"])
                row["id"] = entry["id"]
                report["queries"].append(row)
            report['warm_repeats'] = []
            for repeat in range(5):
                for entry in specification['queries']:
                    row = query(corpus, entry['text'])
                    report['warm_repeats'].append({'repeat': repeat, 'id': entry['id'], 'elapsed_ms': row['elapsed_ms']})
            times = sorted(r['elapsed_ms'] for r in report['warm_repeats'])
            report['warm_p95_ms'] = times[math.ceil(.95 * len(times)) - 1]
            report['latency_scope'] = 'native CLI spawn through decoded results; 5 ordered repeats of all37; no host cache flush; no model-unload cold phase applies to Baloo'
            report['index_size_bytes'] = sum(p.stat().st_size for p in (sandbox / 'data').rglob('*') if p.is_file())
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
        finally:
            if daemon and daemon.poll() is None:
                os.killpg(daemon.pid, signal.SIGTERM)
                try:
                    daemon.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(daemon.pid, signal.SIGKILL)
                    daemon.wait(timeout=5)
            report["daemon_closed"] = daemon is not None and daemon.poll() is not None
            report["daemon_exit_code"] = daemon.returncode if daemon else None
            stop_sample.set()
            if sampler:
                sampler.join(timeout=2)
    (sandbox / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    return 1 if "error" in report else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.session:
        return session(args.session.resolve())
    if not args.output:
        parser.error("--output is required")
    output = args.output.resolve()
    if output.exists():
        parser.error("preserving existing result: choose a new output directory")
    run(["sha256sum", "-c", "benchmark/SHA256SUMS"], timeout=10)
    sandbox = Path(tempfile.mkdtemp(prefix="baloo-retrieval-", dir=ROOT / "local"))
    for name in ["home/corpus", "config", "data", "cache", "runtime", "empty-config"]:
        (sandbox / name).mkdir(parents=True, exist_ok=True)
    (sandbox / "runtime").chmod(0o700)
    for source in (ROOT / "benchmark/corpus").iterdir():
        shutil.copyfile(source, sandbox / "home/corpus" / source.name)
    shutil.copyfile(ROOT / "benchmark/queries.json", sandbox / "queries.json")
    corpus = sandbox / "home/corpus"
    (sandbox / "config/baloofilerc").write_text(f"[Basic Settings]\nIndexing-Enabled=true\n[General]\nfolders={corpus}\nonly basic indexing=false\nindex hidden folders=false\nexclude filters=\nexclude filters version=99999\n")
    env = {"PATH": "/usr/bin:/bin", "LC_ALL": "C", "HOME": str(sandbox / "home"),
           "XDG_CONFIG_HOME": str(sandbox / "config"), "XDG_CONFIG_DIRS": str(sandbox / "empty-config"),
           "XDG_DATA_HOME": str(sandbox / "data"), "XDG_DATA_DIRS": "/usr/local/share:/usr/share",
           "XDG_CACHE_HOME": str(sandbox / "cache"), "XDG_RUNTIME_DIR": str(sandbox / "runtime"),
           "BALOO_DB_PATH": str(sandbox / "data/baloo"), "QT_QPA_PLATFORM": "offscreen"}
    command = ["bwrap", "--ro-bind", "/", "/", "--bind", str(sandbox), str(sandbox),
               "--tmpfs", "/tmp", "--unshare-pid", "--unshare-net", "--proc", "/proc", "--dev", "/dev",
               "--die-with-parent", "--", "dbus-run-session", "--", "/usr/bin/python",
               str(Path(__file__).resolve()), "--session", str(sandbox)]
    began = time.perf_counter()
    child = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)
    try:
        stdout, stderr = child.communicate(timeout=125)
    except subprocess.TimeoutExpired:
        os.killpg(child.pid, signal.SIGKILL)
        child.communicate(timeout=5)
        raise RuntimeError("Baloo sandbox exceeded its bounded deadline; terminated")
    report = json.loads((sandbox / "results.json").read_text())
    report.update({"sandbox_closed": child.poll() is not None, "sandbox_exit_code": child.returncode,
                   "elapsed_seconds": time.perf_counter() - began,
                   "corpus_inventory_sha256": hashlib.sha256((ROOT / "benchmark/SHA256SUMS").read_bytes()).hexdigest(),
                   "isolation": "read-only host root; only owned sandbox writable; separate PID/network namespace and session bus"})
    output.mkdir(parents=True)
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    shutil.copyfile(sandbox / "daemon.log", output / "daemon.log")
    (output / "session.log").write_text((stdout + stderr).replace(str(sandbox), "<sandbox>"))
    print(json.dumps({"queries": len(report["queries"]), "error": report.get("error"), "closed": report["daemon_closed"] and report["sandbox_closed"]}))
    return child.returncode


if __name__ == "__main__":
    os.chdir(ROOT)
    sys.exit(main())
