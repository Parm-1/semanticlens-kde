# Release candidate acceptance audit

The public repository is created and its initial hosted CI passed. Release
publication remains unfinished while linked SemanticLens packages and final hosted CI finish.
The tested code build is source `5beacaebb65427478e7e2dd820c8cbea64cf13ea`.
The SemanticLens successor changes only visible GUI/runner strings and package
metadata. Its affected build and runtime hashes are recorded separately; service,
producer and CLI bytes must remain identical to the accepted CPU stage.

| Goal requirement | Current authoritative evidence | Result and limits |
|---|---|---|
| Linux text/image inference correctness | `evidence/public-producer-parity-v2`: exact staged CPU executable, model/runtime identities, eight unchanged raw vectors, negative/malformed/recovery controls | Passed on CPU; GPU remains disabled/unvalidated. |
| Fixed redistributable retrieval benefit and exact identifiers | `benchmark/` CC0 inventory; `evidence/retrieval-v2`; independent `evidence/retrieval-review-v2` | 20/20 semantic top-five, all six identifiers first, four labeled PDF pages; +100 points against reused Baloo v1 typed-query baseline. Authored corpus, no general photo/population claim; 15 no-match suggestions retained. |
| Incremental edits/rename/delete/exclusions/interruption/restart | `evidence/release-lifecycle-v1`: unchanged actual-worker runner on exact stage, 74 checks; `evidence/combined-storage-review-v2` affected uncertainty/watch/status review | Passed at documented synthetic phases; exact row/body and recovery controls preserved. Routine fixture setup failure retained separately. |
| Native app, PDF preview, KRunner, open/reveal, simultaneous clients | `evidence/combined-native-v1`, `public-install-v2`, prior independently reviewed `desktop-open-review-v1`, `desktop-boundary-v2`, `runner-privacy-review-v1` | Current installed app/standard plugin discovery/concurrent requests and current compiled native preview pass. Open/reveal use stated original-file/private-handler tests; arbitrary handlers, stock Enter and physical input/presentation are not claimed. |
| Selected-root privacy, local storage and bounded cooperative work | `combined-storage-review-v1/v2`, `local-root-v1/repair-v2/v4`, final lifecycle and native incomplete-notice receipts | Storage/descriptor/traversal limits, source association, no default roots, exclusion/cancellation, visible incomplete status and idle worker closure covered. Trusted driver/backing-stack/remount and hard-RSS/blocking-I/O limitations retained. |
| Intended KDE environment and clean installation | `public-clean-build-v2`, `public-install-v2`, `metadata-stage-v1` | Full source CPU build and actual owned Arch `/usr` install/default discovery/removal with host Wayland; metainfo validates offline. Other distributions and a booted fresh desktop unqualified. |
| Responsiveness and resource observations | `retrieval-v2` and independent review; final lifecycle status/idle checks | Warm p95 47.236 ms; three model-reload queries 1.708–2.341 s. Reported sampled RSS excludes GUI and lacks a trace; transient index size differs from closed DB. No causal comparison of different clocks or scaling extrapolation. |
| Source, notices, assets, reproducible packaging and CI | `PUBLIC_SOURCE.md`, `ASSET_PROVENANCE.md`, `LICENSE`, notices/reference texts; release-v3 assembler, repeated archive readback; local CI-equivalent checks | Public export excludes four copied OS binaries and models/indexes. Retained source/hashes and native notices traced. Bounded payload review identifies package-default snapshots, not user profiles; no universal secret/license guarantee. [Hosted CI passed](https://github.com/Parm-1/semanticlens-kde/actions/runs/38310540856) on initial public commit `4e0b0532478680a9bf10882a80ee98377df83c80`; archive release remains pending. |
| Later audio/video and opt-in code path | `GOAL.md` architecture and current separate worker/segment interfaces | Deferred intentionally as first-release scope; no implemented capability advertised. |
| Public GitHub repository and first publication | [Public `Parm-1/semanticlens-kde`](https://github.com/Parm-1/semanticlens-kde), reviewed initial source commit and hosted CI success | Original approval granted and source published. SemanticLens for KDE and Parm-1/semanticlens-kde selected; native label/metadata checks passed; packaging and release asset publication pending. Goal remains active. |

Assembly and a source push are not completion of the goal. Final identity,
release publication and asset readback still require verification. Preserve the original reports and fixed labels; repairs
or stronger claims require linked evidence, not editing sealed negatives.

The SemanticLens labels-only successor has portable native readback in
[semanticlens-branding-v2](../evidence/semanticlens-branding-v2/note.md). The
new stage has four expected changed files and 17 unchanged retained files.
An extra broad helper timeout is preserved; its relative socket configuration
is explained by [independent source-faithful startup controls](../evidence/semanticlens-startup-v1/report.md): relative-directory socket rejection before Engine startup, with absolute-socket readiness and clean closure. The broader helper is not claimed as passing.
