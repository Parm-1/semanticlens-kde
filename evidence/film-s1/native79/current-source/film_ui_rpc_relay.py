#!/usr/bin/env python3
"""Transparent newline-JSON Unix-socket relay with paired peer/timing receipts."""
from __future__ import annotations

import json
import os
import select
import socket
import struct
import threading
import time
from pathlib import Path


class RpcRelay:
    clock = "CLOCK_MONOTONIC via time.monotonic_ns()"

    def __init__(self, listen_path: str, upstream_path: str, records_path: str,
                 hold_query_text: str = "", release_query_text: str = ""):
        self.listen_path = listen_path
        self.upstream_path = upstream_path
        self.records = Path(records_path).open("x", encoding="utf-8", buffering=1)
        self.log_lock = threading.Lock()
        self.stop_event = threading.Event()
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(listen_path)
        os.chmod(listen_path, 0o600)
        self.listener.listen(16)
        self.listener.settimeout(0.2)
        self.next_connection = 0
        self.next_sequence = 0
        self.hold_query_text = hold_query_text.casefold()
        self.release_query_text = release_query_text.casefold()
        self.held_reply: tuple[bytes, dict] | None = None
        self.hold_used = False
        self.held_lock = threading.Lock()
        self.threads: list[threading.Thread] = []
        self.accept_thread = threading.Thread(target=self._accept, name="film-rpc-relay", daemon=True)

    def start(self) -> None:
        self.accept_thread.start()

    def _log(self, row: dict) -> None:
        row["clock"] = self.clock
        with self.log_lock:
            self.records.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
            self.records.flush()

    @staticmethod
    def _peer(front: socket.socket) -> dict:
        pid, uid, gid = struct.unpack("3i", front.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
        try:
            exe = os.readlink(f"/proc/{pid}/exe")
        except OSError:
            exe = "unavailable"
        return {"pid": pid, "uid": uid, "gid": gid, "exe": exe}

    def _accept(self) -> None:
        while not self.stop_event.is_set():
            try:
                front, _ = self.listener.accept()
            except TimeoutError:
                continue
            except OSError:
                break
            try:
                back = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                back.connect(self.upstream_path)
            except OSError as exc:
                front.close()
                self._log({"event": "upstream_connect_failed", "mono_ns": time.monotonic_ns(), "error": str(exc)})
                continue
            with self.log_lock:
                self.next_connection += 1
                connection = self.next_connection
            thread = threading.Thread(target=self._proxy, args=(front, back, connection),
                                      name=f"film-rpc-{connection}", daemon=True)
            self.threads.append(thread)
            thread.start()

    def _proxy(self, front: socket.socket, back: socket.socket, connection: int) -> None:
        peer = self._peer(front)
        upstream_peer = self._peer(back)
        buffers = {front: bytearray(), back: bytearray()}
        pending: dict[str, dict] = {}
        try:
            while not self.stop_event.is_set():
                readable, _, exceptional = select.select([front, back], [], [front, back], 0.2)
                if exceptional:
                    break
                for source in readable:
                    data = source.recv(65536)
                    if not data:
                        return
                    target = back if source is front else front
                    now = time.monotonic_ns()
                    buffers[source].extend(data)
                    while b"\n" in buffers[source]:
                        line, _, remainder = buffers[source].partition(b"\n")
                        buffers[source] = bytearray(remainder)
                        frame = bytes(line) + b"\n"
                        try:
                            value = json.loads(line)
                        except (UnicodeDecodeError, json.JSONDecodeError):
                            self._log({"event": "invalid_json_frame", "connection": connection,
                                       "direction": "client_to_service" if source is front else "service_to_client",
                                       "peer": peer, "frame_mono_ns": now, "raw_frame": frame.decode("utf-8", "replace")})
                            continue
                        if source is front:
                            target.sendall(frame)
                            forwarded_ns = time.monotonic_ns()
                            key = str(value.get("id", ""))
                            with self.log_lock:
                                self.next_sequence += 1
                                sequence = self.next_sequence
                            pending[key] = {
                                "sequence": sequence,
                                "connection": connection,
                                "peer": peer,
                                "upstream_peer": upstream_peer,
                                "request_start_mono_ns": now,
                                "request_frame_mono_ns": now,
                                "request_forwarded_mono_ns": forwarded_ns,
                                "request": value,
                                "raw_request": frame.decode("utf-8", "replace"),
                            }
                            self._log({"event": "request_received", **pending[key]})
                            self._log({"event": "request_forwarded", "sequence": sequence,
                                       "connection": connection, "request_id": key,
                                       "peer": peer, "upstream_peer": upstream_peer,
                                       "request_forwarded_mono_ns": forwarded_ns,
                                       "request": value, "raw_request": frame.decode("utf-8", "replace")})
                        else:
                            key = str(value.get("id", ""))
                            record = pending.pop(key, None)
                            if record is None:
                                record = {"connection": connection, "peer": peer, "upstream_peer": upstream_peer,
                                          "request_id": key, "request": None}
                            record.update({"reply_mono_ns": now, "reply": value,
                                           "raw_reply": frame.decode("utf-8", "replace")})
                            request = record.get("request") or {}
                            text = request.get("text", "")
                            with self.held_lock:
                                should_hold = (not self.hold_used and self.held_reply is None and self.hold_query_text
                                    and request.get("op") == "film_search"
                                    and isinstance(text, str) and text.casefold() == self.hold_query_text
                                    and value.get("ok") is True)
                                if should_hold:
                                    self.held_reply = (frame, record)
                                    self.hold_used = True
                            if should_hold:
                                self._log({"event": "successful_reply_held", **record,
                                           "held_raw_reply": frame.decode("utf-8", "replace")})
                                continue
                            target.sendall(frame)
                            forwarded_ns = time.monotonic_ns()
                            self._log({"event": "reply_forwarded", **record,
                                       "reply_forwarded_mono_ns": forwarded_ns})
                            if (value.get("ok") is True and self.release_query_text and request.get("op") == "film_search"
                                    and isinstance(text, str) and text.casefold() == self.release_query_text):
                                with self.held_lock:
                                    held = self.held_reply
                                    self.held_reply = None
                                if held is not None:
                                    held_frame, held_record = held
                                    target.sendall(held_frame)
                                    released_ns = time.monotonic_ns()
                                    self._log({"event": "held_successful_reply_delivered_after_fresh_reply",
                                               "connection": held_record.get("connection"),
                                               "request_id": (held_record.get("request") or {}).get("id"),
                                               "peer": held_record.get("peer"),
                                               "upstream_peer": held_record.get("upstream_peer"),
                                               "request": held_record.get("request"),
                                               "reply": held_record.get("reply"),
                                               "raw_reply": held_record.get("raw_reply"),
                                               "held_raw_reply": held_frame.decode("utf-8", "replace"),
                                               "reply_mono_ns": held_record.get("reply_mono_ns"),
                                               "reply_forwarded_mono_ns": released_ns,
                                               "fresh_request_id": key,
                                               "fresh_reply_forwarded_mono_ns": forwarded_ns})
        except OSError as exc:
            self._log({"event": "proxy_socket_error", "connection": connection,
                       "peer": peer, "mono_ns": time.monotonic_ns(), "error": str(exc)})
        finally:
            for request in pending.values():
                request.update({"event": "request_without_reply", "closed_mono_ns": time.monotonic_ns()})
                self._log(request)
            for item in (front, back):
                try:
                    item.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                item.close()

    def close(self) -> None:
        self.stop_event.set()
        try:
            self.listener.close()
        except OSError:
            pass
        self.accept_thread.join(timeout=2)
        for thread in self.threads:
            thread.join(timeout=2)
        with self.held_lock:
            held = self.held_reply
            self.held_reply = None
        if held is not None:
            frame, record = held
            self._log({"event": "held_successful_reply_not_delivered_before_relay_close",
                       "connection": record.get("connection"),
                       "request_id": (record.get("request") or {}).get("id"),
                       "peer": record.get("peer"), "upstream_peer": record.get("upstream_peer"),
                       "request": record.get("request"), "reply": record.get("reply"),
                       "raw_reply": record.get("raw_reply"),
                       "held_raw_reply": frame.decode("utf-8", "replace"),
                       "reply_mono_ns": record.get("reply_mono_ns"),
                       "close_mono_ns": time.monotonic_ns()})
        self.records.close()
        try:
            os.unlink(self.listen_path)
        except OSError:
            pass
