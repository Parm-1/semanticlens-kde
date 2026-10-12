# First Linux CPU text/image embedding proof

Outcome: the pinned DigUp pipeline runs on the intended Linux CPU and all eight
applicable upstream text/image reference vectors pass its frozen >=0.999 dot
agreement requirement. This candidate is delivered for independent review and
parent acceptance; its author does not approve release compatibility.

## Contract and original evidence

DigUp source commit: `6faa29e4c315a3a591de1b41bd78751bdf39588d`, reverified at
`/tmp/digup-kde-assessment-20261011-source`. Exact copied producer, prompts, model
pins, tests, patch and synthetic-generation source are under `source/` with MIT
notice. `inputs/vectors.json` is the unchanged upstream vector-version 1 reference,
made with llama.cpp b11461. It is serialized upstream evidence, not a locally made
expectation or a direct mlx-vlm run. Five text inputs and three images were
reproduced; its speech vector is retained in the reference file but not tested.

llama.cpp tag b11461 resolves to `4d756bc72bf00a4aacf410ae15a2d315f3db400d`.
The only source change is DigUp's no-logits patch, SHA-256
`33c8c4812a4307aef3f398e5ba622edcc54ef421054edd98ca0f71906b9a2d7e`.
The actual Git diff has the same SHA-256. `build-options.txt` records Release,
CPU/OpenMP, native=OFF and CUDA/Vulkan/Metal/BLAS=OFF. `build.log` ends in linking
both executables. The functioning model and vision encoder, parity and retrieval
below are runtime evidence beyond build/configuration flags.

Models from `ggml-org/embeddinggemma-2-GGUF`, revision
`bfcd298762cc34d0357ece5ebdd31791a3a374d8`, checked against ModelFiles.swift:

| File | Bytes | SHA-256 |
|---|---:|---|
| embeddinggemma-2-Q8_0.gguf | 309855456 | 2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135 |
| mmproj-embeddinggemma-2-Q8_0.gguf | 554821024 | c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f |

Only these public models were downloaded. Weights remain ignored local files.
The binary accepts model paths and **does not itself verify their hashes**.
`embedding_setup.sh` does verify sizes and hashes, and this run's receipt is
`model-sha256.txt`. `info()` declares hardcoded expected model/runtime/pipeline
identities; it must not be interpreted as attesting arbitrary supplied weights.
The application consumer must enforce checked model hashes before trusting that
identity, or add in-worker verification before adopting this protocol.

Text producer: query `task: search result | query: TEXT`; document
`title: TITLE-or-none | text: TEXT`; tokenizer add_special/parse_special=true
(BOS/EOS); truncation retains final EOS at 2048/full or 512/text-only. Up to 16
sequences share the batch, with independent positions. Images use no task prompt.
A whole mixed input's text tokens and vision rows go through one noncausal
`llama_process(ENCODE)` call. Mean pooling covers all output positions, followed
by double-accumulated L2 normalization to 768 floats.

Image producer: mtmd's file helper decodes JPEG/PNG into RGB; application sizing
fills the requested soft-token budget with aspect preserved, dimensions floored
to multiples of 48, and PIL-compatible bicubic with 22-bit fixed-point weights.
mtmd accepts final dimensions with min_tokens=1/max_tokens=1120 and does model
normalization/encoding. `sizing-results.json` contains four exact matches against
upstream stored PIL pixel arrays: shrink, grow, one-axis and identity. No Python
Pillow installation or regeneration of its expected pixels was used.

The actual frozen photo is 1024x768, despite a nearby test comment discussing a
square photo. Runtime geometry was observed, not inferred from that comment:

| Input | Final width x height | Actual encoder rows | Embedding seconds |
|---|---|---:|---:|
| screen-280 | 1008 x 624 | 273 | 7.0305 |
| photo-140 | 624 x 480 | 130 | 2.4022 |
| frame-70 | 432 x 336 | 63 | 1.2243 |

## Checks and observed outcomes

`run.jsonl` is the original 28-response record; `results.json` derives comparisons
and ranking from those actual vectors. Upstream tests compute raw dot because
vectors are normalized; both dot and independently normalized cosine are saved.
The evaluator applies the original raw-dot threshold to each positive.

| Reference | Raw dot | Cosine |
|---|---:|---:|
| query | 0.99991938 | 0.99991940 |
| query-bn | 0.99992954 | 0.99992958 |
| query-ar | 0.99992118 | 0.99992122 |
| document | 0.99979171 | 0.99979178 |
| document-long | 0.99992140 | 0.99992135 |
| screen-280 | 0.99993680 | 0.99993676 |
| photo-140 | 0.99992746 | 0.99992749 |
| frame-70 | 0.99991302 | 0.99991308 |

Contradictory source-affecting inputs falsify parity: omitting the query prompt
has cosine 0.92362058 with query reference; using screen budget 70 has cosine
0.95157847 with screen-280 reference and changes geometry to 480x288/60 rows.
These are actual model outputs, not checker flags. Invalid budget 71, missing
image, string-valued budget and missing query text produce explicit errors with
no vector. A subsequent valid query matches at cosine 0.99991834, witnessing
recovery. A separate text-only/512 process matches query at the same cosine.

`inputs/retrieval.json` freezes eight candidates and eight queries with labels,
criterion and limits before inference. It contains six new synthetic short text
documents and the two public synthetic upstream images. Six semantic/image queries
(all four text and both image descriptions) retrieve their relevant item at rank
1; the deliberately unique identifier ZXQ-7491 is separately rank 1. Every score
and full ranking is in results. The no-relevant query still yields a nearest
candidate (identifier at 0.622372): no abstention threshold or no-hit claim is
established. This small diagnostic is not the release retrieval benchmark, a
Baloo comparison, or evidence of general top-five recall.

Count categories: full run 28 responses = 1 info, 23 successful vector requests
(27 vectors because the reference-text batch has five), and 4 explicit failures.
Plus 1 text-only vector in its separate process. Eight vectors are upstream
parity positives, two are parity negatives, four are pixel-fixture cases.
No private folder was indexed, and no Baloo/package/system configuration changed.

## Measurements, reusable interface and limits

Host: Linux x86_64, AMD Ryzen 5 3600, six worker threads. Full process elapsed
29.6871 s; user CPU 153.3141 s; system CPU 1.6684 s; peak RSS 1,330,164 KiB
(~1.27 GiB). Load plus media warmup and two text warmups: 2.0671 s. The text-only
process elapsed 1.0925 s, peak RSS 433,424 KiB. `run-time.json` and
`query-only-time.json` state the exact accounting scope: sole child in a fresh
Python runner, Linux resource.getrusage and perf_counter around spawn through
wait. GNU time was unavailable; the failed measurement-launch attempt is retained
as `measurement-launch-failure.log` and did not run inference. Initial overlapping
build invocations were interrupted before one serialized successful build;
no output from the interrupted attempts supports the delivered build claim.

The eight warm diagnostic query requests ranged 0.0296–0.0390 s for inference.
This excludes ranking/UI/IPC, is a single small run with uncontrolled background
host activity, and establishes no release p95. Image throughput and long-text
latency require background work. No idle inference occurs while stdin blocks;
this process exits on EOF. Cancellation/concurrency and release worker retention
remain untested.

`probe/README.md` documents the reusable JSONL interface and commands. The binary
supports info, prompted query/document, already-prompted texts and image with an
explicit budget/path. Input errors have no vector. It is one sequential process
whose model/contexts/batch/bitmaps/chunks are owned with RAII. Caller supplies
explicitly selected file paths. It is a foundation, not a production IPC/privacy
boundary: no request bounds, root enforcement, sandbox, cancellation, authentication,
concurrent clients, index lifecycle, extraction or restart contract is established.
Advertised budget code supports 70/140/280/560/1120; evidence covers only 70/140/280.
Truncation implementation follows source, but oversized-input boundary and all
16-sequence capacity cases were not separately tested. Audio/video and GPU are
unvalidated. Model distribution approval and publication belong to the parent/user.

Setup/build: `./tools/embedding_setup.sh`. Recheck without touching sealed evidence:
`python tools/embedding_run.py evidence/embedding-v1/inputs/requests.jsonl build-probe/recheck`.
The setup checks the pinned source/diff, model integrity and runs CMake/Ninja;
`setup-check.log` records a successful repeated setup/build verification. Both
executables retained identical binary hashes after that build, so the original
inference and pixel receipts are reused.
Reusing unchanged receipts is sufficient unless source/inputs/backend changes.

## Outcome branches and stop rule

- If independent review confirms the frozen candidate and parent accepts its
  narrow compatibility contract, reuse this producer as the CPU foundation,
  binding persisted vectors to verified model hashes and pipeline identity.
- If review finds an affected defect, repair within the package in a linked
  successor evidence version and return the affected claims to the same uninvolved
  reviewer. Keep this evidence immutable.
- If a future pinned backend falls below 0.999 on an applicable input, do not mix
  vectors or silently loosen criteria. Keep the known runtime or make an explicit
  new pipeline identity and re-embedding decision through the parent.

Stop rule for this package: eight applicable parity positives, meaningful frozen
retrieval, falsifying preprocessing cases, failure/recovery cases and reusable
commands are checked. No wider corpus tuning, personal indexing, GPU work or UI
commission is inferred. The next dependency is independent parent-dispatched
review/acceptance, not more speculative tests.

Resource closure: clone/download/build and both inference processes exited; no
worker/compiler/process handle needs resuming. No children were spawned. Source
and model files remain local for the consumer. Implementation/model API billing
usage is unavailable; the recorded process timings are CPU/wall scope, not model
billing or total project elapsed. Independent review usage is outside this package.
The report, source/input/result receipts and exact candidate copies are frozen by
`SHA256SUMS`; candidate root files are separately pinned by `candidate-sha256.txt`.
