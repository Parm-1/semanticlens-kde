# Independent Linux CPU embedding review v1

Verdict: supported for the narrow first Linux CPU compatibility claim. No demonstrated defect blocks the five text and three image reference positives, four exact resize fixtures, diagnostic retrieval, or observed per-input error/recovery claim. This is review evidence for parent acceptance, not acceptance or authorization to adopt the probe as a production service.

The reviewer did not author or design the candidate. Candidate, sealed evidence and canonical state were left unchanged. The only public write is this report; replay receipts are under ignored `local/embedding-review-v1/`.

## Exact identity and custody

Reviewed `evidence/embedding-v1/` and its frozen `candidate/probe` and `candidate/tools` against live root files. All 12 live candidate pins match their frozen snapshots; both executable pins and both actual model-file hashes match. Decisive sealed identity pins were independently recalculated:

- Author report: `000b1f4339c094c6ef981d9706d12de3a1f635ee95c9cc2f412cb0e2884ef35a`.
- Results: `79fe9065a7ad82c076555b010f7f6c2b9b4bd8e2503dda9e139e92df05e4c1db`.
- Candidate pin list: `de50312ef738e04ca06f9036ac65c9816c1a0132b06c28d09e969a2ec9897252`.
- Evidence SHA256SUMS: `b144ed4b97ddf6a517eee8463d0028e63c5941caa83459d22430f6a364a95358`.

The parent's already checked 52-entry sealed inventory is reused. Independently compared nine copied producer/test/model/build/input-generator source files and `vectors.json`, `screen.png`, `photo.jpg` byte-for-byte with Git objects at DigUp `6faa29e4c315a3a591de1b41bd78751bdf39588d`. Actual llama.cpp HEAD is `4d756bc72bf00a4aacf410ae15a2d315f3db400d`; its actual diff equals the frozen no-logits patch, SHA256 `33c8c4812a4307aef3f398e5ba622edcc54ef421054edd98ca0f71906b9a2d7e`. Executable embedding-probe SHA256 is `f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`.

## Producer, ABI and applicable phases

`source/Model.swift:88-99` establishes actual query/document framing and no media prompt. `source/LlamaEmbedder.swift:60-82,127-137,162-188,233-295` establishes full/text-only limits, sequence capacity, final-EOS truncation, mixed placement, pooled output and double-accumulated normalization. The candidate matches these operations at `candidate/probe/main.cpp:28-41,49-54,58-88,99-101`, with the intentional CPU backend change.

Actual vendor ABI checks establish that `llama_batch_ext::set_token_embd` copies rows (`src/llama-batch.cpp:1200-1237`), so mtmd-owned row lifetime is sufficient. `llama_context::encode` takes the whole batch in one microbatch and forces noncausal encoder attention (`src/llama-context.cpp:1480-1551`); `src/llama-graph.cpp:267-310` weights all tokens per sequence for mean pooling. These are source conclusions, corroborated by final native outputs, rather than conclusions from info flags alone. RAII owns loaded model/context/media/batch and image chunks/bitmaps; sequential EOF exit was observed. No asynchronous lifetime or concurrent-client conclusion follows.

Application sizing and 22-bit bicubic match copied `source/ImageSizing.swift`; actual native sizing-check replay equals all upstream stored byte arrays: shrink 36 bytes, grow 180, one-axis 42, identity 105. These are native fixture checks against upstream serialized PIL expectations, not a new Pillow/reference-runtime run. Original and replay geometry agree: screenshot 1008x624/273 rows; photo 624x480/130; frame 432x336/63. The photo fixture is actually 1024x768; the upstream square-photo comment is not evidence.

## Raw evidence challenge and reproduction

Original `run.jsonl` has 28 unique, correctly ordered response IDs matching actual requests: one info, 23 successful vector requests yielding 27 vectors, four failures. The separate original text-only process adds one vector. All eight reference positives are finite 768-vectors. Independently recomputed raw dots with upstream serialized vectors; minimum is document `0.9997917117943279`, above the unchanged `0.999` threshold in `source/ReferenceVectorTests.swift:67-73`. `made_with=llama.cpp b11461` identifies serialized upstream reference evidence; this review establishes no direct mlx-vlm or upstream native execution.

Contradictory actual raw inputs falsify parity: prompt omission raw dot `0.9236205334598584`; screenshot budget 70 raw dot `0.951578417611135` and geometry 480x288/60 rows. The evaluator saves cosine for these controls, but independent raw-dot computation establishes that both also fail the applicable raw-dot criterion. Invalid budget 71, absent file, string budget and missing text yield errors without vector fields. Subsequent valid query raw dot is `0.9999183576524665`; original text-only query has the same raw dot. This covers the observed early input failures, not recovery from a simulated encoder/allocation fault.

Recomputed every diagnostic ranking from actual vectors, rather than trusting evaluator flags: six semantic/image queries and the unique identifier rank their labeled item first. Visually inspected both synthetic image fixtures and confirmed relevance of their descriptions. No-relevant query still returns nearest identifier at `0.6223720043572223`; abstention remains unproved. This supports a meaningful eight-candidate diagnostic, not general recall, exact-keyword preservation, a Baloo comparison or release ranking acceptance.

Replayed the exact 28 requests against the pinned native executable. All eight raw dots, both negatives, errors, recovery, geometry and first-ranked items reproduced the original values. Original observations therefore have direct native corroboration. Replay exit code is zero.

## Actionable integration batch and limits

No repair to this frozen narrow proof is requested. Before product adoption, the integration owner must enforce verified model bytes and persist complete model/runtime/preprocessing identities. `candidate/probe/main.cpp:45-57,94-95` accepts arbitrary model paths but declares hardcoded expected identities; `candidate/tools/embedding_setup.sh:19-32` verifies the intended models only in setup. Treating `info()` as attestation would be unsafe. The author report explicitly acknowledges this, so it is an integration requirement rather than a misrepresented self-attestation defect.

Keep the recorded production exclusions: selected-root enforcement, request bounds, cancellation/stale queries, concurrency, authentication/sandboxing, index migration and lifecycle, extraction, UI responsiveness and idle-worker retention require their own implementation and target validation. Budgets 560/1120, 16-sequence capacity, oversized-input truncation boundary, GPU and audio are code/source possibilities without separate target coverage here. Nothing in this review grants those claims or publication authority.

## Scope, accounting and closure

Original fresh-runner accounting is 29.6871 s wall, 153.3141 s user, 1.6684 s system, peak RSS 1,330,164 KiB. Reviewer replay is 29.6630 s wall, 150.7124 s user, 1.4414 s system, peak RSS 1,342,960 KiB; spawn through wait includes load/warmups/requests/teardown. Replay was for correctness, with uncontrolled background work, and does not establish performance equivalence or release p95. Model/tool inspection is not performance evidence. Total review elapsed and model/API billing usage are unavailable; subprocess time is not billing evidence.

Replay receipts: `local/embedding-review-v1/run.jsonl` SHA256 `926c306fdb4dbedecaf8efb895415dcc4562897d557943a5bc3c445ec8241b44`; `run.log` `ace97c19fe4705caa9d75415c191e02124ad4eb73c5f2c7df8c9ee8412728d44`; `run-time.json` `0ac86e19422db80019eac83e6a20b20952e2ed3ca3f5cf6400cb4b15eb797145`.

Both reviewer child processes exited, and a final /proc command inventory found no embedding-probe, sizing-check, ninja or cmake process. No children were delegated. No Baloo settings, personal folder, system configuration or source state was changed. Recheck the affected claims and interactions with this same reviewer if the author supplies a linked successor; parent retains contract, acceptance and integration decisions.
