# Portable engine v2 review delivery

This linked delivery makes the accepted affected review inspectable in a public clean checkout without ignored `local/` evidence. It changes no review conclusion, candidate, canonical state or acceptance boundary.

Original affected report: `evidence/engine-review-v2/report.md`, SHA256 `b3b4c868f7b342aa650ecdebe4d898d8e4e565e16b8bad126ce2a3c8c16465f4`. Original review pins: `1be17fcaa4c9f9dbc552b72d5a116150e135cd816e25ea80fc122588cbcb7bf5`. Parent accepted the affected service repairs within that report's limits.

All copied files are byte-identical to their originals. Original engine review v1/v2 pinned dependencies were verified before copying; the three foundation replay receipts additionally match the explicit hashes in the retained embedding review report. The old reports and pin manifests are copied unchanged as historical identity records. Their old relative paths remain historical: verify this delivery with `sha256sum -c pins.sha256` from this directory, and resolve report references using the mapping below.

The package includes all 20 distinct ignored engine-review script/raw dependencies from the two review manifests, all three ignored foundation replay receipts named by the retained embedding review, and seven unchanged reports/manifests. Existing checkout companions `evidence/engine-v1/`, `evidence/engine-v2/`, `evidence/embedding-v1/` and `evidence/embedding-review-v1/` retain the candidate, source and original producer evidence. No referenced ignored raw receipt is omitted.

No model weights, model link, native runtime binary, SQLite database, runtime root, personal source file, credential or secret is copied. Historical raw observations retain synthetic workspace absolute path prefixes and old PIDs for byte fidelity; they contain synthetic test passages and timing/protocol/SQL observations. This is a portable inspection delivery, not a claim that the historical scripts run from this directory without their original prerequisites.

For native replay only, excluded prerequisites are `build-probe/embedding-probe` (reviewed SHA256 `f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`), `models/embeddinggemma-2-Q8_0.gguf`, `models/mmproj-embeddinggemma-2-Q8_0.gguf`, the frozen service candidates, and synthetic runtime artifacts generated in the original script sequence. In particular, the historical startup script reads `local/engine-review-lifecycle-v2/runtime/data/index.sqlite`; that database is intentionally excluded, while its decisive SQL observations and producing script are included. These prerequisites are not needed to inspect the delivered claims/raw evidence.

No native controls were rerun, no live resource was created, and no old artifact was edited. Original recorded process/socket closure is unchanged. Acceptance and public publication remain parent/user decisions.

## Original-to-portable mapping

All destinations below are relative to this directory.

| Original checkout path | Portable copy |
| --- | --- |
| `evidence/embedding-review-v1/report.md` | `originals/evidence/embedding-review-v1/report.md` |
| `evidence/engine-review-v1/pins.sha256` | `originals/evidence/engine-review-v1/pins.sha256` |
| `evidence/engine-review-v1/report.md` | `originals/evidence/engine-review-v1/report.md` |
| `evidence/engine-review-v2/pins.sha256` | `originals/evidence/engine-review-v2/pins.sha256` |
| `evidence/engine-review-v2/report.md` | `originals/evidence/engine-review-v2/report.md` |
| `evidence/engine-v1/pins.sha256` | `originals/evidence/engine-v1/pins.sha256` |
| `evidence/engine-v2/pins.sha256` | `originals/evidence/engine-v2/pins.sha256` |
| `local/embedding-review-v1/run-time.json` | `receipts/local/embedding-review-v1/run-time.json` |
| `local/embedding-review-v1/run.jsonl` | `receipts/local/embedding-review-v1/run.jsonl` |
| `local/embedding-review-v1/run.log` | `receipts/local/embedding-review-v1/run.log` |
| `local/engine-review-lifecycle-v1/events.jsonl` | `receipts/local/engine-review-lifecycle-v1/events.jsonl` |
| `local/engine-review-lifecycle-v2/events.jsonl` | `receipts/local/engine-review-lifecycle-v2/events.jsonl` |
| `local/engine-review-startup-v1/events.jsonl` | `receipts/local/engine-review-startup-v1/events.jsonl` |
| `local/engine-review-v1/closure.json` | `receipts/local/engine-review-v1/closure.json` |
| `local/engine-review-v1/lifecycle_controls-v2.py` | `receipts/local/engine-review-v1/lifecycle_controls-v2.py` |
| `local/engine-review-v1/lifecycle_controls.py` | `receipts/local/engine-review-v1/lifecycle_controls.py` |
| `local/engine-review-v1/run_controls-v2.py` | `receipts/local/engine-review-v1/run_controls-v2.py` |
| `local/engine-review-v1/run_controls.py` | `receipts/local/engine-review-v1/run_controls.py` |
| `local/engine-review-v1/startup_controls.py` | `receipts/local/engine-review-v1/startup_controls.py` |
| `local/engine-review-v2-commit/events.jsonl` | `receipts/local/engine-review-v2-commit/events.jsonl` |
| `local/engine-review-v2-index/actual-extracted-pages.txt` | `receipts/local/engine-review-v2-index/actual-extracted-pages.txt` |
| `local/engine-review-v2-index/events.jsonl` | `receipts/local/engine-review-v2-index/events.jsonl` |
| `local/engine-review-v2-recheck/closure.json` | `receipts/local/engine-review-v2-recheck/closure.json` |
| `local/engine-review-v2-recheck/commit_control.py` | `receipts/local/engine-review-v2-recheck/commit_control.py` |
| `local/engine-review-v2-recheck/events.jsonl` | `receipts/local/engine-review-v2-recheck/events.jsonl` |
| `local/engine-review-v2-recheck/index_reload_pdf.py` | `receipts/local/engine-review-v2-recheck/index_reload_pdf.py` |
| `local/engine-review-v2-recheck/recheck.py` | `receipts/local/engine-review-v2-recheck/recheck.py` |
| `local/engine-review-v2/producer-requests.jsonl` | `receipts/local/engine-review-v2/producer-requests.jsonl` |
| `local/engine-review-v2/producer-responses.jsonl` | `receipts/local/engine-review-v2/producer-responses.jsonl` |
| `local/engine-review-v2/raw-controls.json` | `receipts/local/engine-review-v2/raw-controls.json` |
