# Independent engine v1 review

Outcome: request same-author repair of five demonstrated defect categories before integrating these claims. The frozen CPU foundation and selected-root enforcement remain supported in the bounded controls. This review does not accept the owner’s candidate, change the contract, or authorize publication.

The reviewer did not author or substantially design this candidate. No candidate, canonical state, frozen benchmark input, model weight, producer source, Baloo or desktop state was changed. All runtime controls used the exact archived executable and explicitly owned synthetic roots/data/sockets. No child agents were dispatched.

## Exact identity

All 17 relative entries in `evidence/engine-v1/pins.sha256` verified from that directory both before and after review. Reused the unchanged embedding inventory and accepted independent CPU review in `evidence/embedding-review-v1/report.md`; full upstream parity was not rederived.

- Engine service: `f41633a233a61a4592ae5350b8659401d6fbaf5c8cd4c8d73fffe2c1ed282d12`.
- Engine CLI: `943c8e89146ce007a283db304795d507def23d823f933633a53e33af103a8c35`.
- Author report: `c3baaab9f193649aa3beca2cdaa4d731e1fbefca975b2ebc966e2755160bc1bc`.
- Author results: `91cee6894cc6ea6b45332412662138a516590658bad72f2081a1adff338221c1`.
- Author pin manifest: `c7b06ebf5c8bc090cd9e016f07001487d1215c65b90f43d79796e064ce0c85ec`.
- Actual unchanged native producer: `f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`.

Review evidence is bound by this directory’s `pins.sha256`; raw receipts remain under the pinned `local/engine-review-*` paths. These are synthetic native integration controls, not benchmark tuning, game evidence, or new serialized-reference/native-upstream parity claims.

## Producer, observer and accepted phases

Read the actual producer framing/truncation source and frozen core, server, client, CMake, contract and integration helper. `probe/main.cpp:99-101` frames queries and documents differently and uses image budget preprocessing independently. The accepted prior review establishes the actual pooling/vector ABI, image resize and CPU source applicability. Core `vectorBlob` requires 768 finite normalized float32 values; persisted identity includes the actual producer SHA, verified model hashes, pipeline, dtype and byte order. Actual model bytes are checked at bootstrap and every producer reload, independent of producer `info` flags. SQLite belongs to one worker; commit and explicit generation ingress share its mutex. Source snapshots, source digest recheck and authorization recheck precede accepted insertion.

Independently inspected the author’s 239 raw replies: 239 unique ids, 22 successful search replies, three startup negatives with readiness false, actual PDF page 2/OCR page 1 passages, image result paths, cancellation without results and persisted revisions. The seven diagnostic labels support only that authored diagnostic corpus. Checker flags were not substituted for producer evidence.

Reviewer ranking/identity run accepted seven synthetic files/eight vectors, with hidden, credential, generated and outside-link content absent from raw SQL rows. An independent query/document request pair to the pinned producer reproduced the stored `z-literal.txt` vector byte-for-byte. Its native query/stored-vector raw dot is `0.8112404964861316`; vector SHA256 is `b3d984ba014c52423da01a0578a21a862bd1c6546f33885b0ab7bf75f9718b7d`. This positively establishes actual caller framing/storage on the reviewed candidate, rather than inferring it from the service’s semantic-score field. Exact/FTS negatives below use contradictory source passages and raw returned fields.

After acknowledged exclusion, SQL went from seven files/eight segments/revision 8 to six files/seven segments/revision 9, and search omitted the excluded literal file. After acknowledged wipe, SQL and search were empty, with revision 16. These are accepted-control phases. Ordinary selected-root exclusion, symlink avoidance, no default roots, snapshots, source-page association and revision invalidation are supported for this bounded fixture. An adversarial same-user mutation sandbox, clean installation and corpus-wide responsiveness are not established.

## Actionable same-author batch

### R1 — P1: FTS near-matches displace a unique literal identifier

Frozen core `engine.cpp:791-847` places literal substring and FTS hits in the same `exact:true,score:1` bucket, then uses path order. In `local/engine-review-v2/raw-controls.json`, event `literal-vs-fts`, query `ZXQ-7491`, limit 5 returns only `a0-near.txt` through `a4-near.txt`. Each passage contains `ZXQ 7491`, not the literal identifier, yet each reports `exact:true,score:1`. The sole complete-passage literal file `z-literal.txt` is omitted. FTS punctuation normalization therefore overwhelms the stated exact-identifier goal even on seven eligible files. This behavior is consistent with the draft ENGINE field definition, but fails the stronger GOAL invariant; the draft ranking is explicitly pending review.

Repair the ranking contract so complete literal identifiers retain priority over token/FTS near-matches, and preserve a control with at least five near-matches. Recheck best-segment selection, ordering, kind filtering and the public meaning of `exact`; parent owns any field/contract change.

### R2 — P1: segment boundaries erase literal identifier matching

Core `engine.cpp:688-711` takes disjoint 1800-character pieces; exact/FTS search sees only segment text/title. The owned `boundary.txt` contains `ZXQ-7491` starting at offset 1797. Raw accepted segment lengths are 1800 and 55, neither contains the literal. Event `excluded-negative` returns that file with `exact:false,score:0.760772829064959` despite the literal source identifier. This is separate from token truncation and occurs within every documented file/segment limit.

Preserve complete exact-text/identifier retrieval across this boundary without fabricating a source passage/page. Recheck the boundary’s literal result and source association. If segmentation/vector preprocessing or persisted representation changes, version the affected identity/schema and show that old and new representations cannot silently mix.

### R3 — P2: reload identity failure leaves readiness falsely positive

Core `engine.cpp:260-266,350-352,974-983` updates readiness at bootstrap but not when reload validation fails. In `raw-controls.json`, the producer’s ordinary idle exit is observed; an owned model-path symlink is then redirected to an owned wrong-size fixture. Model weights themselves were never changed. Search returns `ok:false,error:"model byte size mismatch"`, while immediately subsequent status reports `ready:true,model_ready:true,error:""`. The unchanged SQL index stays at seven files/eight segments/revision 8, and restoring the owned link allows query recovery.

Actual byte rejection is sound; readiness/error observability is misleading. Repair status to reflect the identity/inference failure and deliberate successful recovery. Recheck both query and indexing callers of reload validation, with no accepted vectors on failure. Do not weaken the byte gates.

### R4 — P2: rejected or canceled controls leave scheduling effects

Core `engine.cpp:149-156` sets pause/resume state and increments generation before worker validation/cancellation. In `local/engine-review-lifecycle-v2/events.jsonl`, the actual service processes pipelined pause/cancel requests during indexing: cancel returns `accepted:true`, pause returns `ok:false,error:"canceled"`, yet status stays `paused:true`. SQL has no persisted `paused` row, so this canceled action also creates process/restart divergence. The controlled long file has no accepted partial row.

In the same run, `set_exclusions:["../outside"]` returns `invalid relative exclusion prefix`. Its ingress generation change cancels the active scan anyway: status becomes `busy:false,total:0,current:<a-big.txt>,error:""`; the eligible big file has no accepted row, while exclusions remain empty. An explicit valid scan recovers. The first disconnect attempt is not proof of canceled-control behavior: its pause had already persisted and is retained separately as an inconclusive timing observation.

Make failed/canceled controls preserve coherent scheduling state, or explicitly restrict cancellation and define committed control semantics before clients depend on them. Recheck these two raw counterexamples and valid pause/resume ingress; do not infer rollback from a canceled reply alone. Any narrowed cancellation contract is a parent decision.

### R5 — P2: fatal startup retains the heavy producer past idle shutdown

Core `engine.cpp:383-419` enters the fatal reply loop without closing the already loaded producer or applying idle expiry. `local/engine-review-startup-v1/events.jsonl` copies a native accepted synthetic database and changes only its owned persisted pipeline identity to a contradictory value. Startup correctly refuses it (`ready:false,model_ready:false,error:"stored identity mismatch; use a fresh data directory"`). After a measured 11.000083812-second idle interval, producer PID 605353 remains alive. Ordinary idle closure is 10 seconds. Explicit shutdown then closes it.

Close heavy resources on fatal bootstrap refusal while preserving cheap status/control responses. Recheck actual contradictory stored identity and producer closure; ordinary idle/shutdown controls need only affected interaction checks.

## Material recovery boundary, separate from defects

The parent’s suspected fatal-control hang is not reproduced. In the startup-mismatch run, `wipe`, `scan` and `resume` promptly return the stored-identity error; no request hangs. Wipe leaves the preexisting native accepted file/segment intact. The fatal loop replies without executing any controls. A distinct fresh data directory starts ready, accepts wipe and returns an empty search, matching ENGINE’s documented instruction to use a fresh directory on stored identity mismatch.

Thus refusal preserves identity compatibility, but the current socket alone cannot fulfill deliberate index removal/rebuild after startup refusal. A GUI that exposes only socket wipe needs an explicit removal/rebuild recovery contract for public users; the parent owns that scope decision. Deliberate user-authorized wipe/removal is distinct from silently adopting a changed binary or auto-upgrading incompatible vectors. No changed-binary automatic-upgrade support is inferred or requested here.

## Coverage limits and recheck condition

Source corrects the premise that watchers walk roots on the main thread: `Engine::startScan` traverses directories on the worker. Main-thread `src/service/main.cpp:110-132` copies the watch list and calls native watch installation/removal in bulk. There is a 10000 eligible-file scan bound but no corresponding directory-count bound. Small-corpus status samples and observable add-path failures do not prove latency or coverage for very large directory trees. This is missing target/capacity evidence, not a measured latency defect. Keep the present narrow performance claims; large-corpus/p95 proof remains parent-owned. Queued searches can wait one complete file; no-match cutoff, frozen release recall, GPU, clean install, frontend/Runner and publication acceptance remain outside this review. The parent’s UI TLS/KIconTheme exit failure is not attributed to this engine.

Actual extractor versions are not recorded in persisted identity; changed Poppler/Tesseract output and unchanged-file reuse are not validated upgrade claims. Source fingerprint equality is byte equality, not an extractor/runtime migration proof. Include this boundary when choosing a clean installation/update contract rather than claiming universal compatibility from a hardcoded pipeline marker.

Return a linked, immutable successor from the same author with repaired sources/binary, versioned identities where applicable, raw controls and closure. Recheck R1/R2 ranking/source interactions, R3 readiness/reload callers, R4 accepted/canceled scheduling and R5 fatal resource closure, plus any changed recovery contract. Preserve v1 and these falsifying fixtures. The parent retains contract, acceptance, integration and publication decisions; review success grants no additional authority.

## Accounting and resource closure

Successful reviewer runner wall scopes: ranking/identity 17.310182548 s; decisive cancellation lifecycle 3.160859631 s; startup/refusal/fresh-directory 19.540545132 s. The earlier disconnect timing run was 4.001136043 s and is inconclusive for cancellation. The first ranking recorder failed at final JSON serialization after teardown; its uncaptured observations are not used as proof and were replayed with a corrected reviewer-only recorder. Total review elapsed and model/API billing usage are unknown; these native wall scopes are not additive billing or benchmark evidence. The fatal-idle endpoints were acquired in one native script without model/tool calls inside the interval.

Successful native receipts include 64 ranking-run request/reply pairs, 95 lifecycle request/reply pairs plus two pipelined replies, and 85 startup-run request/reply pairs; these are observation counts, not independent test totals. Every recorded service exited with code 0; every known producer/native helper exited; each owned socket is absent. No live build, service, extractor, producer, delegated child or tool process handle remains. Readonly candidate and author evidence pins still verify. Only ignored synthetic runtime/evidence files and this new review report/pins were written.
