# Independent KRunner v2 affected-claim recheck

Linked predecessor: `evidence/runner-review-v1/report.md`, SHA256 `07d9e877e68a174bb75e78bb859e4fd36ba470b6857f8cab2fe2de0cd8ae919a`. The two original findings are repaired on the tested controls. A new actual-service cold-query failure requires author repair before the ordinary single-query integration claim is accepted. Acceptance and integration remain parent-owned.

## Exact identity and provenance

All 17 `evidence/runner-v2/SHA256SUMS` entries verified before review and again after validation. Original v1's eight pins and review-v1's nine pins also remained valid. Reviewed successor source: `src/runner-v2/runner.cpp`, SHA256 `3f3779881ce4fc62633c54bc07586b074dd33ea1499ace001eda17af0047188f`; loaded native module: `build-runner-v2/libkde-index-runner-v2.so`, SHA256 `7a119683264e17b0ead1131064af9a883c175db9985191d46c334348127101a5`. Existing inventories and the installed KF6 6.30.0 / Qt 6.12.0 ABI were reused. The matching-version native thread/context/activation source and its original pins are in review-v1; these unchanged contracts were not fetched or rebuilt again.

No final service delivery manifest/report existed when the actual test began. A private review copy of `build-engine/kde-index-service` was used: `local/runner-review-v2/producer/kde-index-service`, SHA256 `f41633a233a61a4592ae5350b8659401d6fbaf5c8cd4c8d73fffe2c1ed282d12`. At acquisition this also equaled the staged `evidence/engine-v1/candidate/bin/kde-index-service`. Current service/core/docs were copied read-only into the review directory for the relevant mechanism inspection. Their hashes bind inspected bytes; an author-sealed source/build provenance claim was unavailable. The experiment therefore proves behavior of the exact copied service binary, not acceptance of a mutable producer package or compatibility of a later successor.

Read-only probe: SHA256 `f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`. The actual receipt includes independently computed model hashes and sizes (text 309855456 bytes, SHA256 `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`; image 554821024 bytes, SHA256 `c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f`). The service also reported these identities, CPU, dimension 768, runtime llama.cpp-b11461+no-logits, and its v2 segment pipeline. This is identity evidence, not a new embedding-reference or broad ranking proof.

## Repaired findings

Independent replay `local/runner-review-v2/replay.cpp` is a successor of the original uninvolved review harness, with five additional raw-field controls. It was run against the pinned module through real KF6 RunnerManager, without the author's verifier. `replay.jsonl` records 21 named cases, actual requests/replies, native matches, main-thread ticks and peer-close observations; it is not an assertion-count claim.

- Missing/string/null scores, invalid kind/negative page, fractional page, PDF page 0, text page 2, non-string title and relative path all produced no file matches. Valid text/PDF/image rows were accepted; PDF page 3 and empty image passage/page 0 retained their applicable meaning. Scores -1,0,1 mapped to 0,0.5,1. Exact score 1 with semantic_score -0.5 remained accepted. Numeric score 1.01 was rejected. These positives and contradictory negatives support the consumed-field parser repair.
- Wrong id and wrong protocol now produce an invalid-response diagnostic. A matching `ok:false,error:"wrong vector dimension"` envelope produces an explicit service-error diagnostic. A connected timeout reports a timeout, while the absent socket offers the setup match. Diagnostics other than absence carry `setup:false` and no file path. The source distinguishes these states; the earlier false absence and hidden valid error controls are repaired.
- Native context reset suppresses all matches and the peer observes socket closure 25 ms later, while the manager remains alive another 100 ms. All 18 accepted fixture connections observed peer closure. All runners were on worker threads. Lowercase-prefix and unprefixed/uppercase boundaries remain as specified by v2.

## New demonstrated blocker: cold search never completes in the two tested pastes

`src/runner-v2/runner.cpp:28,67-102` retains an 1100 ms reply deadline. The copied service mechanism in `producer/engine.cpp:260-274,361-368` unloads its producer after 10 seconds of idle, then verifies both weights/probe and launches a fresh producer on demand. Source inspection located this interaction; actual binary observations below establish the failure.

The independent controller `local/runner-review-v2/cold_service.py` started the pinned actual service with CPU threads=6, an isolated socket/data directory and exactly one explicitly selected synthetic text file. Its unique identifier is `RUNNER-COLD-IDENTIFIER-92471`. A raw service search returned the exact source passage/path with score 1 and semantic_score 0.8283785106697219. No personal root, desktop setting or activation was used.

Coldness was observed, not assumed from a sleep: `/proc/<service>/task/*/children` showed the pinned probe child, then no child after 10133 and 10148 ms idle. Status still reported ready=true, indexed=1, segments=1, index_revision=1, and no errors. Each following native invocation pasted the complete query once through real RunnerManager.

| Actual native control | Query interval | Main timer ticks | Outcome |
| --- | ---: | ---: | --- |
| Warm before idle | 45 ms | 4 | Exact file, relevance 1 |
| Single paste after confirmed unload 1 | 1114 ms | 111 | Timeout diagnostic only |
| Warm after cold 1 | 37 ms | 3 | Exact file, relevance 1 |
| Single paste after confirmed unload 2 | 1113 ms | 111 | Timeout diagnostic only |
| Warm after cold 2 | 45 ms | 4 | Exact file, relevance 1 |

Both cold native attempts ended before their newly observed producer could complete. A subsequent raw service request returned the same correct file (692/597 ms after it was submitted, including any remaining queued load/work); subsequent warm native requests returned normally. Those follow-up times are not an isolated model-load measurement and must not be added to the native deadline to claim one. All original timed endpoints are in `cold-service.json`.

The unchanged pasted query has no automatic completion after `match` returns: the adapter closes its request socket at timeout and publishes a diagnostic; it does not schedule a retry. Thus in these two tested cold states the ordinary single-paste operation fails even though the file is indexed and searchable. This is a functional integration defect, not just a missed latency target or an absence-reporting regression. It is not a claim that every possible machine/cold attempt must fail.

Main-loop responsiveness remains supported: 111 ticks occurred during each actual cold timeout. In a separate confirmed-idle actual-service control, reset fired at 105 ms and yielded zero matches through a further 150 ms event-loop observation (total measured 256 ms, 25 ticks). The independent fixture additionally measured peer disconnect. The actual service reset control does not measure immediate producer termination; a later raw request recovered normally.

Repair requirement for the original owner: allow a still-current cold query to reach a valid file response under a declared bounded cold-aware contract, while retaining prompt invalid-context abort, stale-result suppression, truthful failure states and warm-query behavior. Return a linked frozen successor and recheck cold completion after observed unload, warm mapping, and reset during cold wait. The parent should coordinate any producer/adapter contract change. Do not turn the demonstrated warm success into a cold or live-desktop claim.

## Limits and closure

Raw independent evidence: `local/runner-review-v2/replay.jsonl` and `cold-service.json`; replay/controller/native sources and the exact copied producer are pinned by this report's SHA256SUMS. Synthetic response replay, actual CPU service behavior and live desktop behavior remain separate. No Plasma install/discovery, open/reveal, image/PDF inference, broad semantic benchmark, revision-refresh/app concurrency or target latency distribution is established here. No game claim applies. The source activation contract is unchanged from review-v1 and was not executed or re-reviewed as a new claim.

All fixture threads joined and sockets were removed. Actual service shutdown acknowledged and exited 0; its pid and socket are absent. Post-run process inspection found no review native/controller/service process or embedding-probe process. Isolated XDG runtime contains no sockets. There are no child agents or live tool/process handles. Candidate source, v1/v2 evidence, review-v1 and canonical state remained unchanged. Reports and all referenced local evidence are finalized before this notice.

Measured times are the raw steady-clock query/controller intervals and observed child-state endpoint intervals. Hash collection and tool/model roundtrips occurred outside those intervals. Full review elapsed time and token/billing usage were not exposed or measured; no project-wide cost or p95 inference is made.
