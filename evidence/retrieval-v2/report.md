# Frozen retrieval v2 — exported staged service

## Candidate and method

This run evaluates the exact service staged from public source export v2, with no ranking, corpus, query, label or criteria changes. The runner and scorer are the existing retrieval-v1 tools, unchanged at the hashes below. The service ran in a network-isolated Bubblewrap namespace; the Arch bootstrap was read-only except for the assigned `public-clean-v2` subtree, and both model files were read-only mounts. The 28-file benchmark was copied by the runner into a new private runtime corpus. No Baloo command, daemon, configuration or index was changed or rerun.

| Input | SHA256 / identity |
|---|---|
| staged `kde-index-service` | `28c0d5ddf0ab20608ef527dd891cb023ecd2194690b9f281b95533dacd202ebe` |
| staged `embedding-probe` | `fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0` |
| exact source export archive | `6faeb1ed7df43834eef73702265ae7a298c8e9c53b2f5c5b0b6fe865cbfcd928`; source commit `5beacaebb65427478e7e2dd820c8cbea64cf13ea` |
| EmbeddingGemma text model | `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`; `google/embeddinggemma-2`, revision `bfcd298762cc34d0357ece5ebdd31791a3a374d8` |
| EmbeddingGemma image projector | `c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f` |
| runtime/pipeline identity | `llama.cpp-b11461+no-logits` / `digup-vectors1-linux-cpu-probe1`; backend `CPU` |
| frozen corpus/queries/labels/criteria | `benchmark/SHA256SUMS` `2fdb07dba3b3191a7073b7fa1443e58c542fa3c03e3275d429e54058c8996872`; 28 files, 37 queries |
| retrieval runner / scorer | `b34c4f347b5aa446a8bec377d2eebb860d521122a61581414a313431bed247a3` / `1b45cd6170aed29975c0472fe388c8359bd17e241de299dcf4ac17e3bcc59770` |
| candidate input pin file | `78b6d9187c479150a67d5227365584cab29e7c8505b160107fb8151df92f3647` |
| reused Baloo raw baseline | `b2284df09dc184e76a11338b6f766c4de979925494c1f182fb196e1a283a8280`; Baloo 6.30.0, v1 evidence |

## Retrieval results

The unchanged scorer reports 20/20 semantic queries in top five (100%): 8/8 text-document, 4/4 PDF, and 8/8 image queries. Rank one: 6/8 documents, 4/4 PDFs, and 7/8 images. Every semantic PDF result contains its labeled page in the top five (4/4). All six deliberately unique identifiers and all eight literal controls rank first. The reused Baloo v1 raw output has 0/20 semantic top-five hits, so the measured difference against that recorded baseline is +100 percentage points and exceeds the frozen 80% / +15-point targets. This is a cross-run comparison with the prior Baloo evidence, not a same-session or newly collected baseline.

| Semantic modality | Queries | Top five | Rank one | Query IDs and relevant rank |
|---|---:|---:|---:|---|
| Text documents | 8 | 8/8 | 6/8 | s01=1, s02=2, s03=1, s04=2, s05=1, s06=1, s07=1, s08=1 |
| PDF | 4 | 4/4 | 4/4 | s09=1, s10=1, s11=1, s12=1; labeled page correct in top five: 4/4 |
| Images | 8 | 8/8 | 7/8 | s13=1, s14=1, s15=1, s16=1, s17=2, s18=1, s19=1, s20=1 |

PDF page results:

| ID | Labeled file/page | Returned relevant file/page and rank |
|---|---|---|
| s09 | p01.pdf page 2 | p01.pdf page 2 rank 1 |
| s10 | p02.pdf page 2 | p02.pdf page 2 rank 1 |
| s11 | p03.pdf page 2 | p03.pdf page 2 rank 1 |
| s12 | p04.pdf page 2 | p04.pdf page 2 rank 1 |

Exact and literal query IDs:

- Unique identifiers (6/6 top five; all rank one): e01=1, e02=1, e03=1, e04=1, e05=1, e06=1.
- Literal controls (8/8 top five; all rank one): l01=1, l02=1, l03=1, l04=1, l05=1, l06=1, l07=1, l08=1.

Confusing semantic cases were retained in the raw scores: `s02` ranks the refund document second behind `d12.txt` (0.7568 vs 0.7503); `s04` ranks the labeled document second behind image `i03.png` (0.7606 vs 0.7582); `s17` ranks labeled image `i05.png` second behind visually related `i06.png` (0.8024 vs 0.7969). These are concrete near-match errors with narrow score gaps, not reasons to tune the frozen candidate.

All three no-relevant queries return five unrelated suggestions each (15 false positives). Their raw top-five scores are retained below; no abstention threshold has been validated.

| ID | Unrelated top-five scores |
|---|---|
| n01 | 0.6967, 0.6924, 0.6913, 0.6868, 0.6844 |
| n02 | 0.6978, 0.6934, 0.6879, 0.6872, 0.6863 |
| n03 | 0.7349, 0.7041, 0.7040, 0.7026, 0.6976 |

## Runtime and resource observations

The runner sent 37 initial searches, 185 warm repetitions (five ordered passes over the same 37 queries), and three fixed cold searches: 225 search exchanges total, excluding protocol status calls. The cold controls wait until the service has no live child processes before each query; they are producer-unloaded reload observations, not cold OS-cache measurements.

- Warm p95: **47.236 ms** across 185 exchanges (send through matching decoded reply, including IPC and inference). Frozen target: <=1,000 ms.
- Cold producer-unloaded searches: s01 1707.707 ms, s09 2341.362 ms, s13 2302.849 ms.
- Indexing: 22.566 s for 28 files and 38 vectors; this interval begins at `add_root` and ends when indexing becomes idle, excluding initial model startup. Errors/failures: 0/0.
- Sampled summed service/descendant RSS peak: 1183568 KiB, sampled every 50 ms; shared pages may be counted more than once and short peaks may be missed.
- Transient SQLite index files at the runner endpoint: 1412976 bytes; this is an apparent file-size sum, not allocated disk usage.
- Service exit 0; all recorded PIDs absent; Unix socket absent. The sampler tracks the service and observed descendants; this is not an exhaustive process-tree guarantee.
- Package usage and overall wall-clock scope are unknown. No GPU, broader corpus, user-population, or unique-RSS claim is made.

## Evidence files

- `service/results.json`: all raw top-five paths, pages, scores, query order, timing endpoints, worker identity and runtime status.
- `service/service.log`: service diagnostics.
- `score.json`: unchanged original scoring output against the reused Baloo raw result.
- `candidate-pins.sha256`: staged service/probe, model files, export archive, source tools, frozen benchmark and baseline raw pins.
- `pins.sha256`: delivery pins for this report and all result artifacts.
