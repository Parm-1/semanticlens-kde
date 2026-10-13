#!/usr/bin/env python3
"""Own one dedicated KWin v17 native film-control run and preserve original RPC evidence."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import shlex
import signal
import socket
import subprocess
import sys
import tempfile
import time

from film_ui_rpc_relay import RpcRelay


PINS = {
    "service": "9e102f7e127f7f29419a7cba281b90f03dd81fa9609c534d702eead0533d80df",
    "tokenizer": "bca52b5f2945f2959b7493f3cd6d48f0cbce35547d6e215b95b555f643659a1b",
    "probe": "fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0",
    "runner_helper": "6331f961d570f0a607eaf600465970aea86c5285f3bb9c4ebe9d9c4c980ff0b5",
    "runner_plugin": "a587a20400e4892c0eded5af30c08e646ea09885a971eb3a6a6e6c61bda2e9ba",
    "text_model": "2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135",
    "image_model": "c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f",
}


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def validate_service_sha256(value: str) -> str:
    if not re.fullmatch(r"[0-9a-f]{64}", value or ""):
        raise ValueError("--service-sha256 must be lowercase SHA256")
    return value


def require_service_identity(service: Path, expected_sha256: str, boundary: str) -> str:
    expected_sha256 = validate_service_sha256(expected_sha256)
    actual_sha256 = digest(service)
    if actual_sha256 != expected_sha256:
        raise ValueError(f"{boundary} service SHA256 mismatch: expected {expected_sha256}, actual {actual_sha256}")
    return actual_sha256


def service_sha256_flags(expected_sha256: str) -> list[str]:
    return ["--service-sha256", validate_service_sha256(expected_sha256)]


def validate_optional_film_worker(path: Path | None, expected_sha256: str | None) -> Path | None:
    if bool(path) != bool(expected_sha256):
        raise ValueError("--film-worker and --film-worker-sha256 must be supplied together")
    if not path:
        return None
    path = Path(path).resolve(strict=True)
    if not path.is_file() or not re.fullmatch(r"[0-9a-f]{64}", expected_sha256 or ""):
        raise ValueError("--film-worker must be a file and --film-worker-sha256 must be lowercase SHA256")
    if digest(path) != expected_sha256:
        raise ValueError("film worker SHA256 mismatch before launch")
    return path


def film_worker_flags(path: Path | None, expected_sha256: str | None) -> list[str]:
    return ["--film-worker", str(path), "--film-worker-sha256", str(expected_sha256)] if path else []


def film_worker_binding(path: Path | None) -> list[str]:
    return ["--ro-bind", str(path), "/tmp/test/film-worker"] if path else []


def proc_group(pgid: int) -> list[dict]:
    result = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            raw = (entry / "stat").read_text()
            tail = raw[raw.rfind(")") + 2 :].split()
            if int(tail[2]) == pgid:
                result.append({"pid": int(entry.name), "exe": os.readlink(entry / "exe")})
        except (OSError, ValueError, IndexError):
            continue
    return sorted(result, key=lambda item: item["pid"])


def relay_call(path: Path, request: dict, timeout: float = 10.0) -> dict:
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(timeout)
        client.connect(str(path))
        client.sendall(json.dumps(request, separators=(",", ":")).encode() + b"\n")
        data = bytearray()
        while b"\n" not in data:
            chunk = client.recv(65536)
            if not chunk:
                raise RuntimeError("relay closed during supervisor RPC")
            data.extend(chunk)
        return json.loads(bytes(data).split(b"\n", 1)[0])


def read_relay_rows(records_path: Path) -> list[dict]:
    rows = []
    with records_path.open(encoding="utf-8") as source:
        for line in source:
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return rows


def analyze_source_evidence_rpc(records_path: Path, app_pid: int | None) -> dict:
    candidates = []
    for row in read_relay_rows(records_path):
        request, reply = row.get("request") or {}, row.get("reply") or {}
        reference = request.get("reference") or {}
        evidence = reply.get("evidence") or {}
        valid = (request.get("op") == "film_evidence"
            and (row.get("peer") or {}).get("pid") == app_pid
            and request.get("id", "").startswith("film-ui-")
            and request.get("id") == reply.get("id")
            and reply.get("ok") is True
            and reference.get("film_id") == "film:orchard"
            and evidence.get("reference", {}).get("film_id") == "film:orchard"
            and evidence.get("relative_path") == "orchard/orchard.srt"
            and "CLOCK-7320" in evidence.get("source_excerpt", "")
            and evidence.get("cue_start_ms") == 3000)
        if valid:
            candidates.append(row)
    selected = candidates[-1] if candidates else None
    return {
        "clock": RpcRelay.clock,
        "pass": selected is not None,
        "matching_film_evidence_rpc_count": len(candidates),
        "expected_film_id": "film:orchard",
        "expected_relative_path": "orchard/orchard.srt",
        "expected_payload_substring": "CLOCK-7320",
        "expected_cue_start_ms": 3000,
        "app_peer_pid": app_pid,
        "request_id": (selected.get("request") or {}).get("id") if selected else None,
        "reply_id": (selected.get("reply") or {}).get("id") if selected else None,
        "peer": selected.get("peer") if selected else None,
        "connection": selected.get("connection") if selected else None,
        "request_start_mono_ns": selected.get("request_start_mono_ns") if selected else None,
        "reply_mono_ns": selected.get("reply_mono_ns") if selected else None,
        "raw_request": selected.get("raw_request") if selected else None,
        "raw_reply": selected.get("raw_reply") if selected else None,
    }


def analyze_u02(records_path: Path, result: dict) -> dict:
    u02 = result.get("u02", {})
    job_id = u02.get("background_job_id")
    app_pid = u02.get("ui_peer_pid")
    runner_pid = u02.get("runner_peer_pid")
    rows = read_relay_rows(records_path)

    embedded = []
    for row in rows:
        request, reply = row.get("request") or {}, row.get("reply") or {}
        status = reply.get("status", {})
        request_id = request.get("id")
        if (request.get("op") != "film_status" or reply.get("ok") is not True
                or request_id != reply.get("id") or (row.get("peer") or {}).get("pid") != app_pid):
            continue
        for job in status.get("jobs", []):
            if job.get("job_id") == job_id and job.get("state") == "running" and job.get("stage") == "embed":
                embedded.append({"request_id": request_id, "reply_id": reply.get("id"),
                    "request_start_mono_ns": row.get("request_start_mono_ns"),
                    "reply_mono_ns": row.get("reply_mono_ns"), "peer": row.get("peer"),
                    "connection": row.get("connection"), "job": job})

    def matching(predicate):
        out = []
        for row in rows:
            request, reply = row.get("request") or {}, row.get("reply") or {}
            if (request.get("id") == reply.get("id") and predicate(request, reply)):
                out.append(row)
        return out

    gui_start, gui_end = u02.get("gui_query_start_mono_ns"), u02.get("gui_query_end_mono_ns")
    runner_start, runner_end = u02.get("runner_start_mono_ns"), u02.get("runner_end_mono_ns")

    def inside_action(row, start, end):
        req_ns, rep_ns = row.get("request_start_mono_ns"), row.get("reply_mono_ns")
        return (isinstance(start, (int, float)) and isinstance(end, (int, float))
            and req_ns is not None and rep_ns is not None and start <= req_ns <= rep_ns <= end)

    gui = matching(lambda req, rep: req.get("op") == "film_search"
        and req.get("text", "").casefold() == "clock-7320" and req.get("mode") == "source"
        and req.get("include_unverified_prior") is False
        and rep.get("include_unverified_prior") is False
        and bool(rep.get("results")) and rep["results"][0].get("film_id") == "film:orchard"
        and rep["results"][0].get("source_exact") is True
        and any(item.get("relative_path") == "orchard/orchard.srt" and item.get("cue_start_ms") == 3000
                for item in rep["results"][0].get("evidence", [])))
    gui_owned = [row for row in gui if (row.get("peer") or {}).get("pid") == app_pid
        and row.get("request", {}).get("id", "").startswith("film-ui-")
        and row.get("connection") is not None and inside_action(row, gui_start, gui_end)]

    payment_path = u02.get("legacy_payment_path")
    def runner_request_matches(req, rep):
        if req.get("op") != "search" or req.get("text") != "CARD-39312" or rep.get("ok") is not True:
            return False
        return any(item.get("path") == payment_path and "CARD-39312" in item.get("text", "")
                   for item in rep.get("results", []))
    runner = matching(runner_request_matches)
    runner_exe = u02.get("runner_helper_path", "")
    runner_owned = [row for row in runner if (row.get("peer") or {}).get("pid") == runner_pid
        and (row.get("peer") or {}).get("exe", "").endswith("/" + Path(runner_exe).name)
        and row.get("connection") is not None and inside_action(row, runner_start, runner_end)]

    def bracketed(target: dict, same_connection: bool) -> dict:
        start, end = target.get("request_start_mono_ns"), target.get("reply_mono_ns")
        peer_pid = (target.get("peer") or {}).get("pid")
        target_connection = target.get("connection")
        before = [item for item in embedded if item.get("reply_mono_ns") is not None
            and start is not None and item["reply_mono_ns"] <= start
            and item.get("peer", {}).get("pid") == app_pid
            and (not same_connection or item.get("connection") == target_connection)]
        after = [item for item in embedded if item.get("reply_mono_ns") is not None
            and end is not None and item["reply_mono_ns"] >= end
            and item.get("peer", {}).get("pid") == app_pid
            and (not same_connection or item.get("connection") == target_connection)]
        return {"pass": bool(before and after) and peer_pid is not None and target_connection is not None,
            "target_peer_pid": peer_pid,
            "target_connection": target_connection,
            "target_request_id": (target.get("request") or {}).get("id"),
            "target_reply_id": (target.get("reply") or {}).get("id"),
            "request_start_mono_ns": start, "reply_mono_ns": end,
            "embed_status_before": before[-1] if before else None,
            "embed_status_after": after[0] if after else None}

    gui_interval = bracketed(gui_owned[-1], True) if gui_owned else {"pass": False}
    runner_interval = bracketed(runner_owned[-1], False) if runner_owned else {"pass": False}
    return {
        "clock": RpcRelay.clock, "relay_record_count": len(rows),
        "observed_embed_status_receipts": embedded, "app_peer_pid": app_pid, "runner_peer_pid": runner_pid,
        "ui_owned_source_query_count": len(gui_owned), "runner_owned_legacy_query_count": len(runner_owned),
        "ui_query_interval_bracketed_by_actual_embed_status": gui_interval["pass"],
        "runner_query_interval_bracketed_by_actual_embed_status": runner_interval["pass"],
        "ui_query_interval": gui_interval, "runner_query_interval": runner_interval,
        "ui_query_rpc": gui_owned[-1] if gui_owned else None,
        "runner_query_rpc": runner_owned[-1] if runner_owned else None,
        "gui_action_interval_ns": {"start": gui_start, "end": gui_end},
        "runner_action_interval_ns": {"start": runner_start, "end": runner_end},
        "runner_peer_executable": (runner_owned[-1].get("peer") or {}).get("exe") if runner_owned else None,
        "relay_peers": sorted({(row.get("peer") or {}).get("exe", "") for row in rows}),
    }


def analyze_late_response(records_path: Path, result: dict, ui_pid: int | None) -> dict:
    rows = read_relay_rows(records_path)
    report = result.get("late_response", {})
    old_query = report.get("old_query")
    fresh_query = report.get("fresh_query")
    held = next((row for row in rows if row.get("event") == "successful_reply_held"
        and (row.get("peer") or {}).get("pid") == ui_pid
        and isinstance((row.get("request") or {}).get("text"), str)
        and (row.get("request") or {}).get("text").casefold() == str(old_query).casefold()), None)
    delivered = next((row for row in rows if row.get("event") == "held_successful_reply_delivered_after_fresh_reply"
        and row.get("request_id") == ((held or {}).get("request") or {}).get("id")), None)
    fresh_id = delivered.get("fresh_request_id") if delivered else None
    old_id = ((held or {}).get("request") or {}).get("id")
    cancel = next((row for row in rows if row.get("event") == "request_forwarded"
        and (row.get("peer") or {}).get("pid") == ui_pid
        and (row.get("request") or {}).get("op") == "cancel"
        and (row.get("request") or {}).get("target") == old_id
        and row.get("connection") == (held or {}).get("connection")), None)
    fresh = next((row for row in rows if row.get("event") == "reply_forwarded"
        and (row.get("request") or {}).get("id") == fresh_id), None)
    old_request = (held or {}).get("request") or {}
    old_reply = (held or {}).get("reply") or {}
    old_positive = any(item.get("edition_id") == "edition:harbor:draft-1"
        and any(ev.get("relative_path") == "harbor/harbor.txt" for ev in item.get("evidence", []))
        for item in old_reply.get("results", []))
    fresh_request = (fresh or {}).get("request") or {}
    fresh_reply = (fresh or {}).get("reply") or {}
    fresh_positive = (fresh_request.get("text", "").casefold() == str(fresh_query).casefold()
        and fresh_request.get("mode") == "source"
        and fresh_request.get("include_unverified_prior") is False
        and fresh_reply.get("ok") is True
        and bool(fresh_reply.get("results"))
        and fresh_reply["results"][0].get("film_id") == "film:orchard"
        and fresh_reply["results"][0].get("source_exact") is True)
    exact_unchanged_bytes = bool(held and delivered
        and delivered.get("raw_reply") == held.get("raw_reply")
        and delivered.get("held_raw_reply") == held.get("raw_reply"))
    ordered = bool(fresh and delivered
        and fresh.get("reply_forwarded_mono_ns") is not None
        and delivered.get("reply_forwarded_mono_ns", 0) > fresh.get("reply_forwarded_mono_ns", 0)
        and delivered.get("fresh_reply_forwarded_mono_ns") == fresh.get("reply_forwarded_mono_ns"))
    cancel_after_old_reply_before_fresh = bool(cancel and fresh
        and cancel.get("request_forwarded_mono_ns", 0) > (held or {}).get("reply_mono_ns", 0)
        and cancel.get("request_forwarded_mono_ns", 0) < fresh.get("request_forwarded_mono_ns", 0))
    same_candidate_service_peer = bool(held and fresh
        and (held.get("upstream_peer") or {}).get("pid") == (fresh.get("upstream_peer") or {}).get("pid")
        and (held.get("upstream_peer") or {}).get("exe", "").endswith("/kde-index-service")
        and (fresh.get("upstream_peer") or {}).get("exe", "").endswith("/kde-index-service"))
    passed = (held is not None and old_request.get("op") == "film_search"
        and old_request.get("mode") == "source"
        and old_request.get("include_unverified_prior") is False
        and held.get("request_forwarded_mono_ns") is not None
        and old_positive and old_reply.get("ok") is True and fresh_positive and exact_unchanged_bytes and ordered
        and cancel_after_old_reply_before_fresh and same_candidate_service_peer
        and report.get("old_still_pending_in_ui_before_new_query") is True
        and report.get("stale_old_response_left_fresh_result_unchanged") is True)
    return {"pass": passed, "scope": "service-produced successful raw search reply held by transparent relay, then delivered after distinct fresh reply",
        "ui_peer_pid": ui_pid, "old_query_request": held, "fresh_reply": fresh,
        "held_reply_delivery": delivered, "actual_cancel_request_forwarded": cancel,
        "old_reply_positive_exact_harbor_source": old_positive,
        "fresh_reply_positive_exact_orchard_source": fresh_positive,
        "unchanged_raw_bytes": exact_unchanged_bytes, "fresh_reply_before_old_delivery": ordered,
        "same_connection_cancel_between_old_reply_and_fresh_request": {"pass": cancel_after_old_reply_before_fresh, "row": cancel},
        "same_candidate_upstream_peer_for_old_and_fresh": same_candidate_service_peer,
        "ui_observed_old_pending_before_requery": report.get("old_still_pending_in_ui_before_new_query"),
        "ui_observed_fresh_result_after_old_delivery": report.get("stale_old_response_left_fresh_result_unchanged")}


def analyze_controlled_restart(records_path: Path, result: dict, receipt: dict, ui_pid: int | None) -> dict:
    rows = read_relay_rows(records_path)
    old_pid, new_pid = receipt.get("old_pid"), receipt.get("new_pid")
    restart_report = result.get("controlled_restart", {})
    pairs = [row for row in rows if (row.get("request") or {}).get("id")
        and (row.get("request") or {}).get("id") == (row.get("reply") or {}).get("id")]
    def app_peer(row, pid):
        return ((row.get("peer") or {}).get("pid") == ui_pid
            and (row.get("upstream_peer") or {}).get("pid") == pid)
    old_ui = [row for row in pairs if app_peer(row, old_pid)]
    new_ui = [row for row in pairs if app_peer(row, new_pid)]
    supervisor_shutdown = next((row for row in pairs if (row.get("request") or {}).get("id") == "wrapper-restart-shutdown"
        and (row.get("request") or {}).get("op") == "shutdown"
        and (row.get("peer") or {}).get("pid") != ui_pid
        and (row.get("upstream_peer") or {}).get("pid") == old_pid
        and (row.get("reply") or {}).get("accepted") is True), None)
    capability = next((row for row in new_ui if (row.get("request") or {}).get("op") == "film_capabilities"
        and (row.get("reply") or {}).get("capability") == "film_v1"
        and (row.get("reply") or {}).get("ranking_version") == "semanticlens-film-rrf/2"), None)
    status_rows = [row for row in new_ui if (row.get("request") or {}).get("op") == "film_status"
        and (row.get("reply") or {}).get("status", {}).get("ready") is True
        and (row.get("reply") or {}).get("status", {}).get("model_ready") is True]
    collection_id = restart_report.get("collection_id_before")
    preserved_collection = any(any(item.get("collection_id") == collection_id
        for item in (row.get("reply") or {}).get("status", {}).get("collections", [])) for row in status_rows)
    post_query = next((row for row in new_ui if (row.get("request") or {}).get("op") == "film_search"
        and (row.get("request") or {}).get("text", "").casefold() == "clock-7320"
        and (row.get("request") or {}).get("include_unverified_prior") is False
        and (row.get("request") or {}).get("collection_id") == collection_id
        and bool((row.get("reply") or {}).get("results"))
        and (row.get("reply") or {})["results"][0].get("film_id") == "film:orchard"
        and (row.get("reply") or {})["results"][0].get("source_exact") is True), None)
    old_peer = next((row.get("upstream_peer") for row in old_ui), None)
    new_peer = next((row.get("upstream_peer") for row in new_ui), None)
    pre_receipt = receipt.get("pre_restart_ui_status_receipt", {})
    pre_request = pre_receipt.get("request") or {}
    pre_status = (pre_receipt.get("reply") or {}).get("status", {})
    pre_collection = any(item.get("collection_id") == restart_report.get("collection_id_before")
        for item in pre_status.get("collections", []))
    pre_restart_ui_state = ((pre_receipt.get("peer") or {}).get("pid") == ui_pid
        and (pre_receipt.get("upstream_peer") or {}).get("pid") == old_pid
        and pre_request.get("op") == "film_status" and pre_status.get("ready") is True
        and pre_status.get("model_ready") is True and pre_collection)
    transition_ok = (receipt.get("actor") == "dedicated wrapper"
        and receipt.get("old_pid") != receipt.get("new_pid")
        and receipt.get("old_exit_code") == 0
        and (receipt.get("shutdown_reply") or {}).get("accepted") is True
        and supervisor_shutdown is not None and pre_restart_ui_state
        and receipt.get("socket_released") is True and receipt.get("socket_recreated") is True
        and receipt.get("same_argv") is True and receipt.get("same_data_path") is True
        and receipt.get("same_upstream_socket") is True)
    peer_identity_ok = (old_peer is not None and new_peer is not None
        and old_peer.get("pid") == old_pid and new_peer.get("pid") == new_pid
        and old_peer.get("exe", "").endswith("/kde-index-service")
        and new_peer.get("exe", "").endswith("/kde-index-service"))
    passed = (transition_ok and old_ui and new_ui and capability is not None and status_rows
        and peer_identity_ok and preserved_collection and post_query is not None
        and restart_report.get("disconnect_cleared_prior_opt_in_results_evidence") is True
        and restart_report.get("default_prior_policy_restored") is True)
    return {"pass": bool(passed), "scope": "wrapper-controlled service shutdown, confirmed old PID exit/socket release, same-argv/data/socket relaunch and FilmBackend auto-reconnect",
        "receipt": receipt, "old_ui_upstream_peer": old_peer, "new_ui_upstream_peer": new_peer,
        "pre_restart_ui_status_from_old_peer": pre_receipt,
        "pre_restart_ui_ready_and_collection_valid": pre_restart_ui_state,
        "wrapper_shutdown_request_and_reply": supervisor_shutdown,
        "peer_identity_matches_confirmed_processes": peer_identity_ok,
        "old_peer_request_count": len(old_ui), "new_peer_request_count": len(new_ui),
        "new_peer_capability_reply": capability, "new_peer_ready_status_replies": status_rows,
        "collection_preserved_in_actual_new_peer_status": preserved_collection,
        "fresh_exact_query_from_new_peer": post_query,
        "ui_disconnect_reset_and_reconnect_claim": restart_report,
        "wrapper_process_history": receipt.get("service_history")}

def write_session_launcher(runtime: Path, inner_argv: list[str]) -> Path:
    session = runtime / "session.sh"
    env_writer = (
        "import json,os,sys; keys=('WAYLAND_DISPLAY','DISPLAY','DBUS_SESSION_BUS_ADDRESS',"
        "'XDG_RUNTIME_DIR','HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME','XDG_DATA_HOME',"
        "'XDG_SESSION_TYPE','XDG_CURRENT_DESKTOP'); "
        "open(sys.argv[1],'w').write(json.dumps({k:os.environ.get(k) for k in keys},indent=2)+'\\n')"
    )
    command = ["#!/usr/bin/bash", "set -eu",
               "/usr/bin/python3 -c " + shlex.quote(env_writer) + " /tmp/test/session-env.json",
               "dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_RUNTIME_DIR HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME",
               "exec " + " ".join(shlex.quote(arg) for arg in inner_argv)]
    session.write_text("\n".join(command) + "\n", encoding="utf-8")
    session.chmod(0o700)
    return session


def launch_dedicated(args: argparse.Namespace, root: Path, paths: dict[str, Path]) -> int:
    try:
        require_service_identity(paths["service"], args.service_sha256, "outer")
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    runtime = Path(tempfile.mkdtemp(prefix="slf9-", dir="/tmp"))
    os.chmod(runtime, 0o700)
    for child in ("home", "config", "cache", "data", "state", "runtime", "run", "project"):
        (runtime / child).mkdir(mode=0o700)
    if args.film_worker:
        worker_mountpoint = runtime / "film-worker"
        worker_mountpoint.touch()
        worker_mountpoint.chmod(0o500)
    expected_socket = runtime / "runtime" / "semanticlens-test"
    input_paths = {
        "service": paths["service"], "tokenizer": paths["tokenizer"], "probe": paths["probe"],
        "runner_helper": paths["runner_helper"],
        "runner_plugin": paths["plugin_root"] / "kf6/krunner/kde-index-runner.so",
        "text_model": paths["models"] / "embeddinggemma-2-Q8_0.gguf",
        "image_model": paths["models"] / "mmproj-embeddinggemma-2-Q8_0.gguf",
    }
    actual_pins = {name: digest(path) for name, path in input_paths.items()}
    expected_pins = {**PINS, "service": args.service_sha256}
    if args.film_worker:
        expected_pins["film_worker"] = args.film_worker_sha256
    mismatches = {name: {"expected": expected_pins[name], "actual": actual_pins[name]}
                  for name in PINS if actual_pins[name] != expected_pins[name]}
    if mismatches:
        raise SystemExit("pinned input mismatch: " + json.dumps(mismatches))
    if args.film_worker:
        if digest(args.film_worker) != args.film_worker_sha256:
            raise SystemExit("film worker SHA256 mismatch before launch")
        actual_pins["film_worker"] = args.film_worker_sha256

    def project_path(path: Path) -> str:
        return "/tmp/test/project/" + path.resolve().relative_to(root).as_posix()

    inside = ["/usr/bin/python3", "/tmp/test/project/tools/film_ui_native_v15.py", "--inside",
              "--runtime-root", "/tmp/test/run", "--project-root", "/tmp/test/project",
              "--ui-helper", project_path(paths["ui_helper"]), "--service", project_path(paths["service"]),
              "--tokenizer", project_path(paths["tokenizer"]), "--probe", project_path(paths["probe"]),
              "--models", project_path(paths["models"]), "--runner-helper", project_path(paths["runner_helper"]),
              "--plugin-root", project_path(paths["plugin_root"]),
              "--fixture-root", project_path(paths["fixture_root"]), "--timeout", str(args.timeout)]
    inside += service_sha256_flags(args.service_sha256)
    if args.film_worker:
        inside += film_worker_flags(Path("/tmp/test/film-worker"), args.film_worker_sha256)
    write_session_launcher(runtime, inside)
    bwrap = ["bwrap", "--unshare-user", "--unshare-pid", "--unshare-net", "--die-with-parent",
             "--ro-bind", "/", "/", "--tmpfs", "/home", "--tmpfs", "/run/user", "--tmpfs", "/tmp",
             "--bind", str(runtime), "/tmp/test", "--ro-bind", str(root), "/tmp/test/project",
             "--dev", "/dev", "--proc", "/proc", "--clearenv",
             "--setenv", "PATH", "/usr/bin:/bin", "--setenv", "LANG", "C.UTF-8",
             "--setenv", "HOME", "/tmp/test/home", "--setenv", "XDG_CONFIG_HOME", "/tmp/test/config",
             "--setenv", "XDG_CACHE_HOME", "/tmp/test/cache", "--setenv", "XDG_DATA_HOME", "/tmp/test/data",
             "--setenv", "XDG_STATE_HOME", "/tmp/test/state", "--setenv", "XDG_RUNTIME_DIR", "/tmp/test/runtime",
             "--setenv", "XDG_SESSION_TYPE", "wayland", "--setenv", "XDG_CURRENT_DESKTOP", "KDE",
             "--setenv", "QT_QPA_PLATFORM", "wayland", "--setenv", "QT_QUICK_BACKEND", "software",
             "--setenv", "LIBGL_ALWAYS_SOFTWARE", "1", "dbus-run-session", "--", "kwin_wayland",
             "--virtual", "--width", "1440", "--height", "1000", "--socket", "semanticlens-test",
             "--no-lockscreen", "--no-global-shortcuts", "--no-kactivities", "--exit-with-session",
             "/tmp/test/session.sh"]
    if args.film_worker:
        # Insert the selected worker mapping before the executable; the fixed in-namespace
        # path is what the service argv and receipt record.
        insert_at = bwrap.index("--dev")
        bwrap[insert_at:insert_at] = film_worker_binding(args.film_worker.resolve())
    command_path = runtime / "command.json"
    command_path.write_text(json.dumps(bwrap, indent=2) + "\n", encoding="utf-8")
    log_path = runtime / "compositor.log"
    started_ns = time.monotonic_ns()
    log = log_path.open("wb")
    supervisor = subprocess.Popen(bwrap, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                  start_new_session=True, env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"})
    timed_out = False
    try:
        supervisor.wait(timeout=args.timeout + 60)
    except subprocess.TimeoutExpired:
        timed_out = True
        os.killpg(supervisor.pid, signal.SIGTERM)
        try:
            supervisor.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(supervisor.pid, signal.SIGKILL)
            supervisor.wait(timeout=10)
    log.close()
    try:
        session_env = json.loads((runtime / "session-env.json").read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        session_env = {}
    inner_summary = runtime / "run" / "run-summary.json"
    try:
        inner = json.loads(inner_summary.read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        inner = {}
    process_group_after = proc_group(supervisor.pid)
    desktop_socket_removed = not expected_socket.exists()
    dedicated_environment = (session_env.get("WAYLAND_DISPLAY") == "semanticlens-test"
        and not session_env.get("DISPLAY")
        and session_env.get("XDG_RUNTIME_DIR") == "/tmp/test/runtime"
        and str(session_env.get("DBUS_SESSION_BUS_ADDRESS", "")).startswith("unix:path=/tmp/dbus-")
        and session_env.get("HOME") == "/tmp/test/home"
        and session_env.get("XDG_CONFIG_HOME") == "/tmp/test/config"
        and session_env.get("XDG_CACHE_HOME") == "/tmp/test/cache"
        and session_env.get("XDG_DATA_HOME") == "/tmp/test/data")
    private_host_masks = all(any(bwrap[i:i+2] == ["--tmpfs", mount] for i in range(len(bwrap)-1))
                             for mount in ("/home", "/run/user", "/tmp"))
    summary = {
        "runtime_root": str(runtime), "run_status": "completed" if inner else "failed_or_no_session_result",
        "scope": "dedicated KWin virtual Wayland plus private D-Bus; no host display binding",
        "command": bwrap, "command_sha256": digest(command_path),
        "expected_input_pins": expected_pins, "actual_input_pins": actual_pins,
        "film_worker": ({"path": str(args.film_worker), "sha256": args.film_worker_sha256,
                         "read_only_in_session_path": "/tmp/test/film-worker"}
                        if args.film_worker else None),
        "session_environment": session_env, "dedicated_environment_valid": dedicated_environment,
        "host_display_not_bound": (not session_env.get("DISPLAY")
            and session_env.get("WAYLAND_DISPLAY") == "semanticlens-test"
            and "--clearenv" in bwrap and private_host_masks),
        "dedicated_socket_path": str(expected_socket), "dedicated_socket_removed": desktop_socket_removed,
        "bwrap_pid": supervisor.pid, "bwrap_exit_code": supervisor.returncode,
        "process_group_after": process_group_after, "timed_out": timed_out,
        "inner_summary": inner, "inner_ui_result_pass": inner.get("ui_result_pass"),
        "elapsed_seconds": (time.monotonic_ns() - started_ns) / 1e9,
        "closed": supervisor.poll() is not None and not process_group_after and desktop_socket_removed,
        "usage_measurement_scope": "Harness wall time only; package/token usage unavailable.",
    }
    (runtime / "dedicated-run-summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"runtime_root": str(runtime), "run_status": summary["run_status"],
                      "inner_ui_result_pass": summary["inner_ui_result_pass"],
                      "dedicated_environment_valid": dedicated_environment,
                      "dedicated_socket_removed": desktop_socket_removed,
                      "bwrap_exit_code": supervisor.returncode, "closed": summary["closed"]}, indent=2))
    return 0 if (summary["closed"] and dedicated_environment and not timed_out
                 and inner.get("ui_exit_code") == 0 and inner.get("ui_result_pass") is True
                 and inner.get("u02_relay_phase_proof", {}).get("ui_query_interval_bracketed_by_actual_embed_status")
                 and inner.get("u02_relay_phase_proof", {}).get("runner_query_interval_bracketed_by_actual_embed_status")
                 and inner.get("source_evidence_rpc_proof", {}).get("pass")
                 and inner.get("late_response_proof", {}).get("pass")
                 and inner.get("controlled_restart_proof", {}).get("pass")) else 1


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--project-root", type=Path, required=True)
    p.add_argument("--ui-helper", type=Path, required=True,
                   help="Native QtTest control helper executable (film-ui-controls), not the application executable")
    p.add_argument("--service", type=Path, required=True)
    p.add_argument("--service-sha256", default=PINS["service"],
                   help="Expected service executable SHA256 (default preserves the accepted v15 service)")
    p.add_argument("--tokenizer", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--models", type=Path, required=True)
    p.add_argument("--film-worker", type=Path,
                   help="Optional exact worker executable, exposed read-only inside the dedicated session")
    p.add_argument("--film-worker-sha256", help="Expected SHA256 paired with --film-worker")
    p.add_argument("--runner-helper", type=Path, required=True)
    p.add_argument("--plugin-root", type=Path, required=True)
    p.add_argument("--fixture-root", type=Path, required=True)
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--inside", action="store_true", help=argparse.SUPPRESS)
    p.add_argument("--runtime-root", type=Path, help=argparse.SUPPRESS)
    return p


def main() -> int:
    p = build_parser()
    args = p.parse_args()
    try:
        args.service_sha256 = validate_service_sha256(args.service_sha256)
    except ValueError as error:
        p.error(str(error))
    try:
        args.film_worker = validate_optional_film_worker(args.film_worker, args.film_worker_sha256)
    except (OSError, ValueError) as error:
        p.error(str(error))
    root = args.project_root.resolve()
    paths = {name: getattr(args, name).resolve() for name in
             ("ui_helper", "service", "tokenizer", "probe", "models", "runner_helper", "plugin_root", "fixture_root")}
    if not args.inside:
        return launch_dedicated(args, root, paths)
    if str(args.runtime_root) != "/tmp/test/run":
        raise SystemExit("inside-session runtime must be the private bind-mounted /tmp/test/run")
    try:
        service_sha256 = require_service_identity(paths["service"], args.service_sha256, "inside-session")
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    if args.film_worker:
        if str(args.film_worker) != "/tmp/test/film-worker" or digest(args.film_worker) != args.film_worker_sha256:
            raise SystemExit("inside-session film worker path or SHA256 mismatch")
    session_env = {key: os.environ.get(key) for key in
                   ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR", "HOME",
                    "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_DATA_HOME", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP")}
    wayland_socket = Path("/tmp/test/runtime") / str(session_env.get("WAYLAND_DISPLAY", ""))
    if (session_env.get("WAYLAND_DISPLAY") != "semanticlens-test" or session_env.get("DISPLAY")
        or session_env.get("XDG_RUNTIME_DIR") != "/tmp/test/runtime"
        or not str(session_env.get("DBUS_SESSION_BUS_ADDRESS", "")).startswith("unix:path=/tmp/dbus-")
        or session_env.get("HOME") != "/tmp/test/home"
        or session_env.get("XDG_CONFIG_HOME") != "/tmp/test/config"
        or session_env.get("XDG_CACHE_HOME") != "/tmp/test/cache"
        or session_env.get("XDG_DATA_HOME") != "/tmp/test/data"
        or session_env.get("XDG_SESSION_TYPE") != "wayland"
        or session_env.get("XDG_CURRENT_DESKTOP") != "KDE"
        or not wayland_socket.is_socket()):
        raise SystemExit("refusing native launch outside the verified dedicated KWin/D-Bus session")
    input_paths = {
        "service": paths["service"], "tokenizer": paths["tokenizer"], "probe": paths["probe"],
        "runner_helper": paths["runner_helper"], "runner_plugin": paths["plugin_root"] / "kf6/krunner/kde-index-runner.so",
        "text_model": paths["models"] / "embeddinggemma-2-Q8_0.gguf",
        "image_model": paths["models"] / "mmproj-embeddinggemma-2-Q8_0.gguf",
    }
    actual_pins = {name: digest(path) for name, path in input_paths.items()}
    expected_pins = {**PINS, "service": args.service_sha256}
    if args.film_worker:
        expected_pins["film_worker"] = args.film_worker_sha256
    actual_pins["service"] = service_sha256
    mismatches = {name: {"expected": expected_pins[name], "actual": actual_pins[name]}
                  for name in PINS if actual_pins[name] != expected_pins[name]}
    if mismatches:
        raise SystemExit("pinned input mismatch: " + json.dumps(mismatches))

    runtime = args.runtime_root
    if not runtime.is_dir() or any(runtime.iterdir()):
        raise SystemExit("private /tmp/test/run must exist and be empty")
    data = runtime / "data"
    output = runtime / "out"
    data.mkdir(mode=0o700)
    output.mkdir(mode=0o700)
    upstream, frontend = runtime / "service.sock", runtime / "ui.sock"
    relay_records = runtime / "relay.ndjson"
    service_log_path = runtime / "service.log"
    ui_stdout_path = runtime / "ui.stdout"
    ui_stderr_path = runtime / "ui.stderr"
    service_argv = [str(paths["service"]), "--data-dir", str(data), "--socket", str(upstream),
                    "--probe", str(paths["probe"]), "--probe-sha256", actual_pins["probe"],
                    "--text-model", str(paths["models"] / "embeddinggemma-2-Q8_0.gguf"),
                    "--image-model", str(paths["models"] / "mmproj-embeddinggemma-2-Q8_0.gguf"),
                    "--threads", "2", "--film-tokenizer", str(paths["tokenizer"]),
                    "--film-tokenizer-sha256", actual_pins["tokenizer"]]
    if args.film_worker:
        service_argv += film_worker_flags(args.film_worker, args.film_worker_sha256)
    base_env = os.environ.copy()
    base_env["QT_QPA_PLATFORM"] = "wayland"
    for key, child in (("XDG_CONFIG_HOME", "xdg-config"), ("XDG_DATA_HOME", "xdg-data"),
                       ("XDG_CACHE_HOME", "xdg-cache"), ("XDG_STATE_HOME", "xdg-state")):
        target = runtime / child
        target.mkdir(mode=0o700)
        base_env[key] = str(target)
    service_env = base_env.copy()

    service_logs = [service_log_path.open("wb")]
    service = subprocess.Popen(service_argv, stdin=subprocess.DEVNULL, stdout=service_logs[-1],
                               stderr=subprocess.STDOUT, env=service_env, start_new_session=True)
    service_history = [{"pid": service.pid, "pgid": service.pid, "argv": service_argv,
                        "started_mono_ns": time.monotonic_ns(), "exe": os.readlink(f"/proc/{service.pid}/exe")}]
    relay = None
    ui = None
    ui_group = None
    ui_return = None
    timed_out = False
    started_ns = time.monotonic_ns()
    status = {"runtime_root": str(runtime), "run_status": "started", "clock": RpcRelay.clock,
              "dedicated_session": session_env, "dedicated_wayland_socket": str(wayland_socket),
              "host_display_absent": not session_env.get("DISPLAY"),
              "input_paths": {**{k: str(v) for k, v in input_paths.items()},
                              **({"film_worker": str(args.film_worker)} if args.film_worker else {})},
              "input_pins": actual_pins, "expected_input_pins": expected_pins,
              "film_worker": ({"path": str(args.film_worker), "sha256": args.film_worker_sha256}
                              if args.film_worker else None),
              "service_argv": service_argv, "service_pid": service.pid, "service_pgid": service.pid,
              "service_history": service_history}
    try:
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline and not upstream.exists():
            if service.poll() is not None:
                raise RuntimeError(f"candidate service exited during startup: {service.returncode}")
            time.sleep(0.025)
        if not upstream.exists():
            raise TimeoutError("candidate service did not create its private socket")
        relay = RpcRelay(str(frontend), str(upstream), str(relay_records),
                         hold_query_text="They erased my name from the rescue report.",
                         release_query_text="CLOCK-7320")
        relay.start()
        restart_request_path = runtime / "restart-request.json"
        restart_receipt_path = runtime / "restart-receipt.json"
        base_env["FILM_UI_RELAY_RECORDS"] = "/tmp/test/run/relay.ndjson"
        base_env["FILM_UI_RESTART_REQUEST"] = "/tmp/test/run/restart-request.json"
        base_env["FILM_UI_RESTART_RECEIPT"] = "/tmp/test/run/restart-receipt.json"
        ui_argv = [str(paths["ui_helper"]), str(root), str(paths["service"]), str(paths["probe"]),
                   str(paths["models"]), str(data), str(frontend), str(paths["fixture_root"]), str(output)]
        ui_log_out = ui_stdout_path.open("wb")
        ui_log_err = ui_stderr_path.open("wb")
        ui = subprocess.Popen(ui_argv, stdin=subprocess.DEVNULL, stdout=ui_log_out, stderr=ui_log_err,
                              env=base_env, start_new_session=True)
        ui_group = ui.pid
        status.update({"ui_argv": ui_argv, "ui_pid": ui.pid, "ui_pgid": ui_group,
                       "frontend_socket": str(frontend), "upstream_socket": str(upstream),
                       "data_path": str(data), "output_path": str(output), "relay_records_path": str(relay_records)})
        restart_done = False
        ui_deadline = time.monotonic() + args.timeout
        while ui.poll() is None and time.monotonic() < ui_deadline:
            if not restart_done and restart_request_path.exists():
                request = json.loads(restart_request_path.read_text())
                if request.get("action") != "controlled_upstream_restart":
                    raise RuntimeError("unknown service supervisor request")
                if request.get("ui_pid") != ui.pid or not request.get("nonce") or not request.get("collection_id"):
                    raise RuntimeError("restart request does not match the owned UI and active collection")
                pre_restart_status = next((row for row in reversed(read_relay_rows(relay_records))
                    if (row.get("peer") or {}).get("pid") == ui.pid
                    and (row.get("upstream_peer") or {}).get("pid") == service.pid
                    and (row.get("request") or {}).get("op") == "film_status"
                    and (row.get("reply") or {}).get("status", {}).get("ready") is True
                    and any(item.get("collection_id") == request.get("collection_id")
                        for item in (row.get("reply") or {}).get("status", {}).get("collections", []))), None)
                if pre_restart_status is None:
                    raise RuntimeError("no actual ready UI status receipt from the current upstream peer")
                old_service = service
                old_pid = old_service.pid
                old_entry = service_history[-1]
                stop_ns = time.monotonic_ns()
                shutdown_reply = relay_call(frontend, {"id": "wrapper-restart-shutdown", "op": "shutdown"})
                old_service.wait(timeout=20)
                old_exit_ns = time.monotonic_ns()
                socket_deadline = time.monotonic() + 5
                while upstream.exists() and time.monotonic() < socket_deadline:
                    time.sleep(0.02)
                socket_released = not upstream.exists()
                if not socket_released or old_service.returncode != 0:
                    raise RuntimeError("owned service did not exit cleanly and release its socket")
                new_log = (runtime / f"service-restart-{len(service_history)}.log").open("wb")
                service_logs.append(new_log)
                launch_ns = time.monotonic_ns()
                service = subprocess.Popen(service_argv, stdin=subprocess.DEVNULL, stdout=new_log,
                                           stderr=subprocess.STDOUT, env=service_env, start_new_session=True)
                new_entry = {"pid": service.pid, "pgid": service.pid, "argv": service_argv,
                             "started_mono_ns": launch_ns, "exe": os.readlink(f"/proc/{service.pid}/exe")}
                service_history.append(new_entry)
                socket_deadline = time.monotonic() + 90
                while not upstream.exists() and time.monotonic() < socket_deadline:
                    if service.poll() is not None:
                        raise RuntimeError("replacement candidate service exited during startup")
                    time.sleep(0.025)
                socket_recreated = upstream.exists()
                if not socket_recreated or service.pid == old_pid:
                    raise RuntimeError("replacement candidate service did not create a fresh socket/process")
                receipt = {"action": request["action"], "request": request, "actor": "dedicated wrapper",
                    "clock": RpcRelay.clock, "old_pid": old_pid, "old_exe": old_entry["exe"],
                    "old_exit_code": old_service.returncode, "old_start_mono_ns": old_entry["started_mono_ns"],
                    "shutdown_request_mono_ns": stop_ns, "old_exit_mono_ns": old_exit_ns,
                    "shutdown_reply": shutdown_reply, "socket_released": socket_released,
                    "new_pid": service.pid, "new_exe": new_entry["exe"], "new_start_mono_ns": launch_ns,
                    "pre_restart_ui_status_receipt": pre_restart_status,
                    "socket_recreated": socket_recreated, "same_argv": service_argv == old_entry["argv"],
                    "same_data_path": service_argv[2] == old_entry["argv"][2],
                    "same_upstream_socket": service_argv[4] == old_entry["argv"][4]}
                receipt["service_history"] = service_history
                restart_receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
                status["restart_receipt_path"] = str(restart_receipt_path)
                status["service_history"] = service_history
                restart_done = True
            time.sleep(0.05)
        if ui.poll() is None:
            timed_out = True
            os.killpg(ui_group, signal.SIGTERM)
            try:
                ui_return = ui.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(ui_group, signal.SIGKILL)
                ui_return = ui.wait(timeout=8)
        else:
            ui_return = ui.returncode
        ui_log_out.close()
        ui_log_err.close()
        result_path = output / "results.json"
        result = json.loads(result_path.read_text()) if result_path.exists() else {}
        status.update({"ui_exit_code": ui_return, "timed_out": timed_out,
                       "ui_result_present": bool(result), "ui_result_pass": result.get("pass"),
                       "ui_results_path": str(result_path), "u02": result.get("u02", {})})

        # The wrapper owns cleanup of whichever candidate service is current.
        if service.poll() is None:
            shutdown_reply = relay_call(frontend, {"id": "supervisor-shutdown", "op": "shutdown"})
            status["shutdown_reply"] = shutdown_reply
        try:
            service.wait(timeout=20)
        except subprocess.TimeoutExpired:
            os.killpg(service.pid, signal.SIGTERM)
            try:
                service.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(service.pid, signal.SIGKILL)
                service.wait(timeout=8)
        status["service_exit_code"] = service.returncode
        status["service_history"] = service_history
        if restart_receipt_path.exists():
            status["service_restart_receipt"] = json.loads(restart_receipt_path.read_text())
    except Exception as exc:
        status["failure"] = repr(exc)
        if ui and ui.poll() is None:
            os.killpg(ui.pid, signal.SIGTERM)
            try:
                ui.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(ui.pid, signal.SIGKILL)
                ui.wait(timeout=5)
        if service.poll() is None:
            os.killpg(service.pid, signal.SIGTERM)
            try:
                service.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(service.pid, signal.SIGKILL)
                service.wait(timeout=5)
    finally:
        if relay:
            relay.close()
        for service_log in service_logs:
            service_log.close()
        status["elapsed_seconds"] = (time.monotonic_ns() - started_ns) / 1e9
        status["service_process_groups_after"] = {str(item["pid"]): proc_group(item["pid"]) for item in service_history}
        status["service_process_group_after"] = proc_group(service.pid)
        status["ui_process_group_after"] = proc_group(ui_group) if ui_group else []
        result_path = output / "results.json"
        try:
            result = json.loads(result_path.read_text()) if result_path.exists() else {}
        except json.JSONDecodeError:
            result = {}
        if relay_records.exists():
            evidence_rpc_proof = analyze_source_evidence_rpc(relay_records, ui.pid if ui else None)
            status["source_evidence_rpc_proof"] = evidence_rpc_proof
            (runtime / "source-evidence-rpc-proof.json").write_text(json.dumps(evidence_rpc_proof, indent=2, ensure_ascii=False) + "\n")
            if result:
                phase_proof = analyze_u02(relay_records, result)
                status["u02_relay_phase_proof"] = phase_proof
                (runtime / "u02-relay-proof.json").write_text(json.dumps(phase_proof, indent=2, ensure_ascii=False) + "\n")
                late_proof = analyze_late_response(relay_records, result, ui.pid if ui else None)
                status["late_response_proof"] = late_proof
                (runtime / "late-response-proof.json").write_text(json.dumps(late_proof, indent=2, ensure_ascii=False) + "\n")
                restart_receipt = status.get("service_restart_receipt", {})
                restart_proof = analyze_controlled_restart(relay_records, result, restart_receipt, ui.pid if ui else None)
                status["controlled_restart_proof"] = restart_proof
                (runtime / "controlled-restart-proof.json").write_text(json.dumps(restart_proof, indent=2, ensure_ascii=False) + "\n")
        status["upstream_socket_removed"] = not upstream.exists()
        status["closed"] = (service.poll() is not None and not status["service_process_group_after"]
                            and not status["ui_process_group_after"]
                            and all(not members for members in status.get("service_process_groups_after", {}).values())
                            and status["upstream_socket_removed"])
        status["run_status"] = ("completed_pass" if result.get("pass") is True and not timed_out
                                 else "completed_failed_checks" if result else "failed_or_timed_out_without_result")
        (runtime / "run-summary.json").write_text(json.dumps(status, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"runtime_root": str(runtime), "run_status": status["run_status"],
                      "ui_exit_code": status.get("ui_exit_code"), "ui_result_pass": status.get("ui_result_pass"),
                      "source_evidence_rpc_proof": status.get("source_evidence_rpc_proof"),
                      "u02_relay_phase_proof": status.get("u02_relay_phase_proof"),
                      "late_response_proof": status.get("late_response_proof"),
                      "controlled_restart_proof": status.get("controlled_restart_proof"),
                      "closed": status.get("closed"), "failure": status.get("failure")}, indent=2))
    phase = status.get("u02_relay_phase_proof", {})
    evidence_proof = status.get("source_evidence_rpc_proof", {})
    return 0 if (status.get("closed") and not status.get("failure") and not timed_out
                 and ui_return == 0 and result.get("pass") is True
                 and phase.get("ui_query_interval_bracketed_by_actual_embed_status") is True
                 and phase.get("runner_query_interval_bracketed_by_actual_embed_status") is True
                 and evidence_proof.get("pass") is True
                 and status.get("late_response_proof", {}).get("pass") is True
                 and status.get("controlled_restart_proof", {}).get("pass") is True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
