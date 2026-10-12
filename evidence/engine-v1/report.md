# Local indexing/search engine candidate v1

Outcome: implemented native service and CLI; exact archived executable candidate passed73 direct actual-CPU integration checks. Delivered for parent integration and uninvolved semantic/lifetime/privacy review. The author does not approve release acceptance.

## Candidate and replay

`candidate/` freezes the exact source, CMake, client contract, integration helper and tested service/CLI executables. Root working source remains repairable; never modify this archived candidate, results or report after delivery. Any repair uses linked successor evidence and returns to the same independent reviewer for affected recheck.

Tested service SHA256: `f41633a233a61a4592ae5350b8659401d6fbaf5c8cd4c8d73fffe2c1ed282d12`.
Tested CLI SHA256: `943c8e89146ce007a283db304795d507def23d823f933633a53e33af103a8c35`.
Producer is unchanged reviewed `build-probe/embedding-probe` SHA256 f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf. Reuse unchanged `evidence/embedding-v1` and `evidence/embedding-review-v1` receipts for source/vector parity; this service run is actual native integration, not another upstream-reference parity proof.

Build working source:

```sh
cmake -S engine -B build-engine -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-engine -j4
```

Replay exact candidate (new output directory required):

```sh
python3 evidence/engine-v1/candidate/tools/engine_integration.py --project-root . --service evidence/engine-v1/candidate/bin/kde-index-service --cli evidence/engine-v1/candidate/bin/kde-index-cli --output local/engine-independent-recheck
```

`candidate/docs/ENGINE.md` is the app/Runner contract. Socket uses user-only access, explicit paths/models and no default roots. Controls, FTS/vector results, file/page/visual/OCR passages and status fields are frozen there. Search score is in [-1,1], with exact hits at1 and actual dot separately. The executable expectation can be supplied via `--probe-sha256`; weights always retain reviewed fixed SHA/size gates. Producer info alone is never model verification. Every reload rechecks those bytes.

## Ownership and accepted observation phases

The Qt main event loop owns socket clients, cheap status, cancellation ingress, watcher/debounce and shutdown dispatch. One worker thread exclusively owns SQLite, extraction and sequential full-CPU JSONL producer use. Query/control tasks run between indexing files. Selected roots/exclusions, pause state and identity survive restart. Data-directory lock prevents another service writer.

Files are authorized/canonical and snapshotted under the private data directory, embedded, then original hashes/root applicability and current generation are checked before a single accepted transaction. Old content is removed when replacement work starts; failed/canceled work receives no partial/fabricated successful vectors. Generation changes and commit are serialized with cancellation/control ingress. Producer requests have unique IDs, checked response IDs, successful float32 dimension768/norm/finite validation, and bounded subprocess closure.

Segment INSERT/DELETE triggers advance persisted `index_revision` within accepted SQLite changes. Status refresh publishes the revision after each file/control. Queries carry the revision actually read. Unchanged scans keep it stable; fast removal changes it even if no client saw busy. Clients invalidate and reissue cached queries on revision change. Explicit controls invalidate process-local generations; watcher scans leave that generation stable to avoid false cancellation from duplicate unchanged notifications. Query reads are serialized with writer work.

Native watching covers only authorized paths (500ms synchronization,350ms debounce). Exclusions/removal delete active content before acknowledgement. Hidden/link/outside/default-credential/generated paths and engine data are excluded. OCR produces real text segments; an unreadable image is an observable file error and has zero segments. Empty PDF pages up to page32 are rasterized up to1600 pixels and OCRed; skipped/failed OCR is visible.

## Original direct evidence

`results.json` contains all native request responses, assertion records, actual timing samples and closure. The helper constructs synthetic text/PDF/raster fixtures and copies only the preexisting synthetic embedding images. No personal roots or Baloo state were touched. Initial accepted scan:11 indexed files,13 actual vectors,1 invalid-image error. Hidden/credential/generated/symlink/outside files were absent from accepted rows.

73 checks consist of22 successful search-protocol checks,7 diagnostic labeled top-five checks and44 other retrieval/privacy/lifecycle/identity/CLI checks. The seven labels are reused producer diagnostics, not a new frozen release corpus or benchmark. They include four paraphrases,two visual queries and a unique identifier; separate checks establish exact top-one, PDF page2 and scanned-PDF OCR page1.

Lifecycle checks cover edit, rename, deletion, exclusion, automatic create/edit/delete, unchanged/rapid-delete/restart revision behavior, concurrent cancellation with no results, pause during actual CPU indexing with zero partial segments, persisted pause/exclusions, termination while worker was live, EOF child closure, stale-lock/restart and interrupted snapshot cleanup. The own-data-root and sole-writer controls reject invalid ownership. Raw contradictory weights test both wrong size and correct size/wrong SHA; a contradictory explicit producer pin fails before model readiness. Failed image has no vector, and an outside marker has no outside path in results.

Native host: Linux7.2.7-1-cachyos-bore,x86_64,little-endian; Ryzen5 3600 (6 cores/12 threads), Qt6.12.0; producer uses6 CPU threads. Total runner wall interval:41.084s. Billing/token usage is unavailable. Timings use one monotonic script from request send through decoded response; include cold/warm and lifecycle states. Status:197 samples, maximum0.003476s. Search:22 samples, median0.036686s, maximum2.294840s. These are diagnostic timings; no warm-target p95/RSS/throughput/index-size benchmark acceptance is claimed.

Closure: all recorded service PIDs exited, all known heavy worker PIDs exited, socket absent. Heavy worker also exited after10s idle before service shutdown. No live test/build/IPC handles remain; only ignored synthetic runtime outputs under `local/engine-*` remain.

## Limits, branches and stop rule

This is a tested production foundation with bounded candidate limits, not first-public-release acceptance. One file's extraction/embedding can delay queued searches until that file finishes; status/cancel ingress remains responsive. Extraction has30s subprocess bounds and inference90s bounds, but aggregate multi-page/segment file work can be longer. Large-corpus concurrency/latency requires target evaluation and may need an independent query worker; do not imply this small corpus establishes it. Maximum10000 files per scan,32 OCR page positions,256 segments per file,32MiB source files,2MiB extraction output and1800-character passages apply. Token-dense/multilingual text can still reach the producer2048-token truncation; multilingual coverage is unbenchmarked. Native watcher capacity failures are observable and explicit scan remains available. Filesystem mutation by an adversarial same-user actor is outside the sandbox claim. Float32 byte order is included in identity.

No no-match cutoff is validated: the deliberately absent invoice query returns11 semantic candidates. Release corpus tuning, Baloo comparison, clean installation, GPU, frontend/Runner acceptance and publication remain parent-owned work. Do not claim demonstrated release recall, full-format preservation, adversarial privacy, universal distribution compatibility or target latency from this fixture.

If independent review passes these bounded claims, parent may integrate this candidate and continue the release acceptance work. If a producer/model identity check fails, ready stays false and no vectors are accepted; recover with reviewed artifacts or a fresh identity-bound database rather than weakening gates. If review finds a defect, preserve v1, repair in a linked v2 candidate and recheck affected claims with the same uninvolved reviewer. Stop at changed authority/contract/acceptance or an immutable review dependency; carry independent ready work forward. No direct author/reviewer channel was commissioned here; parent owns dispatch.
