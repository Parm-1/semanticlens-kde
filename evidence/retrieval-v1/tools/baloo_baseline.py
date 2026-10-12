#!/usr/bin/env python3
"""Run an isolated Baloo content-search control; never use the desktop's index.

The writable sandbox is under ignored local/. Bubblewrap makes the host filesystem
read-only, and a new PID/network namespace and session bus contain the daemon.
Requires Linux, bwrap, dbus-run-session, balooctl6, baloosearch6 and balooshow6.
This is a baseline control, not evidence of general semantic retrieval quality.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(args, timeout=10, check=True):
    result = subprocess.run(args, text=True, capture_output=True, timeout=timeout)
    if check and result.returncode:
        raise RuntimeError(f"{args[0]} failed: {result.stderr.strip()}")
    return result


def query(corpus, text):
    timer = time.perf_counter()
    result = run(["baloosearch6", "--id", "--limit", "5", "--directory", str(corpus), text])
    hits = []
    for line in result.stdout.splitlines():
        document_id, absolute = line.split(" ", 1)
        hits.append({"id": document_id, "file": str(Path(absolute).relative_to(corpus))})
    return {"query": text, "hits": hits, "elapsed_ms": 1000 * (time.perf_counter() - timer),
            "stderr": result.stderr.strip()}


def session(sandbox):
    corpus = sandbox / "home" / "corpus"
    files = sorted(corpus.glob("*.txt"))
    result = {"schema": 1, "kind": "Baloo isolated controls", "version": run(["balooctl6", "--version"]).stdout.strip(),
              "scope": "two synthetic text files; no KDE Index retrieval benchmark",
              "source": "KDE/baloo v6.30.0 a5b829873849ce72eb35d2c1b48337b2d81a0593",
              "fixtures": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
              "checks": {}, "queries": [], "daemon_closed": False}
    with (sandbox / "daemon.log").open("w") as log:
        daemon = subprocess.Popen(["/usr/lib/kf6/baloo_file"], stdout=log, stderr=log, start_new_session=True)
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                if daemon.poll() is not None:
                    raise RuntimeError(f"Baloo terminated: {daemon.returncode}; see daemon.log")
                state = run(["balooctl6", "--format", "json", "status", *map(str, files)], check=False)
                if state.returncode == 0:
                    statuses = json.loads(state.stdout)
                    if len(statuses) == len(files) and all(s.get("status") == "done" and s.get("indexing") == "content" for s in statuses):
                        result["content_status"] = [{**s, "file": str(Path(s["file"]).relative_to(corpus))} for s in statuses]
                        break
                time.sleep(0.25)
            else:
                raise TimeoutError("Content indexing did not finish within 60 seconds")
            result["extracted_terms"] = {}
            for file in files:
                result["extracted_terms"][file.name] = run(["balooshow6", "-x", str(file)]).stdout.replace(str(corpus), "<corpus>")
            for text in ["filename:alpha-record", "Domestic", "permission", "Domestic permission",
                         "Domestic telescope", "Domestic OR telescope", '"prior permission"',
                         '"permission prior"', "content:permission", "animals", "keeping a cat"]:
                result["queries"].append(query(corpus, text))
            by_query = {q["query"]: [h["file"] for h in q["hits"]] for q in result["queries"]}
            result["checks"] = {
                "filename_positive": by_query["filename:alpha-record"] == ["alpha-record.txt"],
                "content_positive": by_query["permission"] == ["alpha-record.txt"],
                "implicit_and": by_query["Domestic permission"] == ["alpha-record.txt"] and not by_query["Domestic telescope"],
                "explicit_or": set(by_query["Domestic OR telescope"]) == {"alpha-record.txt", "beta-record.txt"},
                "ordered_phrase": by_query['"prior permission"'] == ["alpha-record.txt"] and not by_query['"permission prior"'],
                "content_field": by_query["content:permission"] == ["alpha-record.txt"],
                "paraphrase_negative": not by_query["keeping a cat"],
            }
            if not all(result["checks"].values()):
                raise RuntimeError(f"Baseline controls failed: {result['checks']}")
        except Exception as error:
            result["error"] = f"{type(error).__name__}: {error}"
        finally:
            if daemon.poll() is None:
                os.killpg(daemon.pid, signal.SIGTERM)
                try:
                    daemon.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(daemon.pid, signal.SIGKILL)
                    daemon.wait(timeout=5)
            result["daemon_closed"] = daemon.poll() is not None
            result["daemon_exit_code"] = daemon.returncode
    (sandbox / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return 1 if "error" in result else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, required=False, help="Fresh directory for frozen public control evidence")
    args = parser.parse_args()
    if args.session:
        return session(args.session.resolve())
    if not args.output:
        parser.error("--output is required")
    output = args.output.resolve()
    if output.exists():
        parser.error("Output already exists; use a new evidence version")
    (ROOT / "local").mkdir(exist_ok=True)
    sandbox = Path(tempfile.mkdtemp(prefix="baloo-control-", dir=ROOT / "local"))
    for directory in ["home/corpus", "config", "data", "cache", "runtime", "empty-config"]:
        (sandbox / directory).mkdir(parents=True, exist_ok=True)
    (sandbox / "runtime").chmod(0o700)
    corpus = sandbox / "home/corpus"
    (corpus / "alpha-record.txt").write_text("Domestic animals require prior permission from the landlord. Reference AX-47291.\n")
    (corpus / "beta-record.txt").write_text("The telescope observation begins after sunset. Reference BX-83720.\n")
    (sandbox / "config/baloofilerc").write_text(
        f"[Basic Settings]\nIndexing-Enabled=true\n[General]\nfolders={corpus}\n"
        "only basic indexing=false\nindex hidden folders=false\nexclude filters=\nexclude filters version=99999\n")
    child_env = {"PATH": "/usr/bin:/bin", "LC_ALL": "C", "HOME": str(sandbox / "home"),
                 "XDG_CONFIG_HOME": str(sandbox / "config"), "XDG_CONFIG_DIRS": str(sandbox / "empty-config"),
                 "XDG_DATA_HOME": str(sandbox / "data"), "XDG_DATA_DIRS": "/usr/local/share:/usr/share",
                 "XDG_CACHE_HOME": str(sandbox / "cache"), "XDG_RUNTIME_DIR": str(sandbox / "runtime"),
                 "BALOO_DB_PATH": str(sandbox / "data/baloo"), "QT_QPA_PLATFORM": "offscreen"}
    command = ["bwrap", "--ro-bind", "/", "/", "--bind", str(sandbox), str(sandbox),
               "--tmpfs", "/tmp", "--unshare-pid", "--unshare-net", "--proc", "/proc", "--dev", "/dev",
               "--die-with-parent", "--", "dbus-run-session", "--", "/usr/bin/python",
               str(Path(__file__).resolve()), "--session", str(sandbox)]
    started = time.perf_counter()
    process = subprocess.Popen(command, env=child_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=85)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate(timeout=5)
        raise RuntimeError(f"Sandbox exceeded deadline and was terminated: {stderr}")
    if not (sandbox / "result.json").exists():
        raise RuntimeError(f"Sandbox did not deliver a result (exit {process.returncode}): {stderr}")
    result = json.loads((sandbox / "result.json").read_text())
    result.update({"sandbox_elapsed_seconds": time.perf_counter() - started,
                   "sandbox_exit_code": process.returncode, "sandbox_closed": process.poll() is not None,
                   "isolation": "read-only host root; writable sandbox only; separate PID/network namespace and session D-Bus"})
    output.mkdir(parents=True)
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    (output / "daemon.log").write_bytes((sandbox / "daemon.log").read_bytes())
    (output / "session.log").write_text(stdout.replace(str(sandbox), "<sandbox>") + stderr.replace(str(sandbox), "<sandbox>"))
    print(json.dumps({"output": str(output), "checks": result["checks"], "error": result.get("error"),
                      "daemon_closed": result["daemon_closed"], "sandbox_closed": result["sandbox_closed"]}))
    return process.returncode


if __name__ == "__main__":
    sys.exit(main())
