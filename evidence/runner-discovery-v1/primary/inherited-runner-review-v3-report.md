# Independent KRunner v3 affected-claim recheck

The prior cold-query blocker is resolved in the two independently observed cold starts below. The parser/error repairs and reset behavior remain supported. No new implementation blocker was found in this affected scope. Acceptance and application integration remain parent-owned. One quantified evidence-description correction is identified below; it does not invalidate the observed cold state or the two successful queries.

## Candidate and inherited evidence

Linked prior review: `evidence/runner-review-v2/report.md`, SHA256 `d7558c6a9552ea9ef523b6693218cf1dad6f24e69f095e4ede15dfc9fad7fc51`. All 20 entries in `evidence/runner-v3/SHA256SUMS` verified before this review and again after validation. Exact reviewed source: `src/runner-v3/runner.cpp`, SHA256 `92909e658246b7cb022f50df9999283e9ce99cf2399f717fe29d14631a5b88eb`; loaded module: `build-runner-v3/libkde-index-runner.so`, SHA256 `dfb9f29e1696ce56fa027a007f6075223c89e1fd3dfc55bbd4833e3e65cb7f31`.

The inspected delta from v2 changes ReplyTimeoutMs from 1100 to 8000 and the metadata filename to `kde-index-runner.json`; metadata/CMake restore the public ID/module `kde-index-runner` / `libkde-index-runner.so`. Native QueryMatch IDs now carry the stable `kde-index-runner_` prefix. This supports physical-module load and metadata identity, not installed discovery. Parser, activation, atomic serial and context/socket ownership code are unchanged. The installed KF6 6.30.0 / Qt 6.12.0 ABI, native KF6 source contract and unchanged source pins from review-v1 were reused. Review-v2's 18 pins verified; predecessor candidate/report inventories were not recreated or edited.

The 8000 ms value is the response-phase budget, checked between waits of at most 30 ms; a final wait and scheduling can cross that budget. Initial connect and write waits remain independently bounded at 250 ms each. The source checks validity throughout the response loop and aborts the stack-owned socket for an invalid context. This is consistent with the previously inspected KF6 shared-context invalidation and runner-thread contract.

## Exact actual producer and cold controls

Only the frozen engine-v1 service was used. Its complete 17-entry `evidence/engine-v1/pins.sha256` inventory verified from that evidence directory. Engine-v1 report SHA256 is `c3baaab9f193649aa3beca2cdaa4d731e1fbefca975b2ebc966e2755160bc1bc`; inventory SHA256 is `c7b06ebf5c8bc090cd9e016f07001487d1215c65b90f43d79796e064ce0c85ec`. The isolated review copy, `local/runner-review-v3/producer/kde-index-service`, matches the frozen binary SHA256 `f41633a233a61a4592ae5350b8659401d6fbaf5c8cd4c8d73fffe2c1ed282d12`. No active engine-v2 binary/source was substituted, built or modified.

The controller is an independent successor of review-v2's controller, with explicit service/probe digest assertions and observed-child closure collection. Probe SHA256 is `f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`. Both existing model files were read-only and independently hashed before measured intervals: text SHA256 `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135` / 309855456 bytes; image SHA256 `c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f` / 554821024 bytes. The service reported the same identities, CPU backend and v2 engine pipeline.

Exactly one explicitly selected synthetic text file contains `RUNNER-REVIEW-V3-IDENTIFIER-69742`. A raw service reply binds the returned path and passage to this source and supplies score 1 with a separate semantic dot. Coldness was verified through `/proc/<service>/task/*/children`: a probe child was observed before idle and no child immediately before the next native query. Present-to-absent observation intervals were 10142.789 and 10138.839 ms before result queries, and 10239.381 ms before reset. These are idle/unload observation intervals, not durations of continuous absence.

Each native query was sent once through the independent real KF6 RunnerManager harness, with no retry:

| Independent actual-service control | Native interval | Main timer ticks | Result |
| --- | ---: | ---: | --- |
| Warm before idle | 43 ms | 4 | Exact file, relevance 1 |
| Cold single paste 1 | 1725 ms | 172 | Exact file, relevance 1 |
| Warm after cold 1 | 57 ms | 5 | Exact file, relevance 1 |
| Cold single paste 2 | 1891 ms | 189 | Exact file, relevance 1 |
| Warm after cold 2 | 56 ms | 5 | Exact file, relevance 1 |

Both cold runs began with no producer child and completed within the response budget. Status before them showed ready=true, indexed=1, segments=1, index_revision=1 and no errors. The exact path/passage, native result data and raw service replies are in `local/runner-review-v3/cold-service.json`; success is not inferred from an author's PASS flag. Both cold calls ran off the main thread and their main timer advanced while waiting.

A third confirmed-idle native request was reset at the observed 94 ms endpoint. It published no matches through a further 150 ms event-loop observation (246 ms total, 24 ticks); a subsequent raw service query returned the expected file. The timer requested 100 ms, but the raw observed endpoint was 94 ms, so 100 ms is not substituted as a measurement. This proves result suppression and main-loop progress on that control, not instantaneous inference-process termination.

## Native error/parser/reset interaction

`local/runner-review-v3/replay.cpp` extends the original uninvolved replay lineage, not the author's checker. Its fixture timeout was extended past the new response budget and its manager guard to 10 seconds. Raw `replay.jsonl` has 24 cases, 21 accepted connections and 21 observed peer closures. It preserves source-shaped text/PDF/image positives and ranking endpoints, including exact score 1 with semantic_score -0.5; contradictory/missing consumed row fields continue to produce no file actions.

The valid failure envelope `{protocol:1,id:<request>,ok:false,error:"wrong vector dimension"}` produces the explicit service-error diagnostic. Removing the error, making it numeric, or changing the request id produces an invalid-response diagnostic. This challenges error-field meaning and identity at the failure-response phase; it does not claim a real probe dimension failure was induced. Wrong success protocol/id likewise produce invalid-response diagnostics, and absence alone offers setup. All other diagnostics carry setup=false and no path.

The synthetic connected timeout returned at 8002 ms with 800 main timer ticks and the truthful timeout diagnostic. Reset in the connected fixture produced no matches; its peer observed socket closure 25 ms after reset, with the manager kept alive another 100 ms. Main-thread timer behavior and the actual cold reset control support responsiveness separately from completed cold retrieval. These results also preserve the previous lowercase-prefix boundary. The installed/live typing race and actions were not exercised.

## Evidence-description correction and remaining limits

The author report says the producer was “absent for” approximately ten seconds before each cold run, and results.json repeats “observed absent for over 10 seconds.” Its raw `idle_before_cold_1/2/reset` records instead have a producer in `before_children` and none in `after_children`, with `observed_idle_ms` spanning that observation loop. They prove that the producer unloaded during the idle interval and was absent before the query; they do not prove ten seconds of continuous absence. Use that narrower statement in parent acceptance/delivery. If the author corrects the frozen prose, use a linked new evidence version rather than editing v3. No inference rerun or implementation change is needed for this description correction.

The practical cold defect is resolved for this exact service/probe/model/host and two exact-text queries. Neither author nor reviewer samples establish a cold distribution, warm p95, semantic/image/PDF recall, simultaneous application/index lifecycle behavior or a live Plasma installation. Stable metadata/module identity is observed; discovery and open/reveal/Okular actions remain untested. No game claim applies. Unchanged activation/producer contracts were not promoted by this adapter recheck.

## Closure and measurement scope

All fixture threads joined; all 21 connected peers observed closure, and named fixture sockets were removed. The actual service acknowledged shutdown and exited 0; its PID/socket are absent. All four observed actual producer PIDs are absent. The native/controller/build handles have completed and there are no reviewer-owned live processes, sockets or children. Parent-owned staged GUI work was separate and was neither controlled nor claimed closed here. Parent was notified that actual cold measured windows/processes had finished before its subsequent heavy work.

Candidate/v1/v2/v3 and predecessor reports were preserved; only the new review's local/build/evidence surfaces were written. Raw results, harnesses, copied service, report and hashes are finalized before delivery. Query timings are steady-clock endpoints acquired in bounded scripts; hashes and model/tool roundtrips were outside those intervals. Full review time and model/tool token/billing usage were not exposed or measured. This report recommends no new code repair in the affected scope and does not accept on the owner's behalf.
