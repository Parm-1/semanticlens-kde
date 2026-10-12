#!/usr/bin/env python3
"""Assert the linked KF6 RunnerManager replay's raw controls and observations."""
import json
from pathlib import Path
import sys

rows = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines() if line]
by_name = {row["case"]: row for row in rows}
assert len(by_name) == len(rows), "duplicate/missing replay case names"

def matches(name):
    return by_name[name].get("matches", [])

def one(name):
    result = matches(name)
    assert len(result) == 1, (name, result)
    return result[0]

for name in ("positive-text", "positive-pdf-fragmented", "positive-image-empty-passage"):
    result = one(name)
    assert result["data"]["path"].startswith("/tmp/kde-index-v3-review-source.")
assert "Page 3" in one("positive-pdf-fragmented")["subtext"]
assert one("positive-image-empty-passage")["data"]["page"] == 0

endpoints = matches("score-endpoints")
assert len(endpoints) == 3
assert [m["relevance"] for m in endpoints] == [0, 0.5, 1]
exact = next(m for m in endpoints if m["data"]["path"] == "/tmp/score-1.txt")
raw_exact = next(r for r in by_name["score-endpoints"]["response"]["results"] if r["path"] == "/tmp/score-1.txt")
assert raw_exact["exact"] is True and raw_exact["score"] == 1 and raw_exact["semantic_score"] == -0.5
assert exact["relevance"] == 1

for name in ("wrong-score-string", "missing-score", "invalid-kind-page", "fractional-pdf-page",
             "zero-pdf-page", "nonzero-text-page", "wrong-title-type", "missing-title",
             "missing-page", "wrong-kind-type", "missing-text", "empty-text", "wrong-path-type", "relative-path"):
    assert matches(name) == [], (name, matches(name))

for name in ("wrong-id", "wrong-protocol"):
    result = one(name)
    assert result["text"] == "KDE Index service returned an invalid response"
    assert result["data"]["setup"] is False
error = one("server-error")
assert error["text"] == "KDE Index service could not complete the search"
assert error["subtext"] == "wrong vector dimension" and error["data"]["setup"] is False
timeout = one("connected-timeout")
assert timeout["text"] == "KDE Index search timed out" and timeout["data"]["setup"] is False
assert by_name["connected-timeout"]["connected"] and by_name["connected-timeout"]["received"]
absent = one("unavailable")
assert absent["text"] == "KDE Index service is not running" and absent["data"]["setup"] is True

reset = by_name["context-reset"]
assert reset["connected"] and reset["received"] and reset["peer_observed_close"]
assert reset["matches"] == [] and 0 <= reset["reset_to_peer_close_ms"] <= 100
assert reset["runner_on_main_thread"] is False
assert by_name["connected-timeout"]["main_heartbeats"] >= 300
assert 7900 <= by_name["connected-timeout"]["match_return_ms"] <= 9500
assert matches("uppercase-trigger") == [] and not by_name["uppercase-trigger"]["connected"]
assert matches("unprefixed") == [] and not by_name["unprefixed"]["connected"]
assert all(row["peer_observed_close"] for row in rows if row["connected"])
print(f"PASS: {len(rows)} RunnerManager cases; {len(rows)} result-envelope and row controls asserted")
