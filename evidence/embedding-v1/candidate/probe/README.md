# Bounded Linux CPU embedding probe

Run from the repository root:

```sh
./tools/embedding_setup.sh
python tools/embedding_run.py evidence/embedding-v1/inputs/requests.jsonl build-probe/recheck
```

The second command writes `build-probe/recheck.jsonl`, `.log` and `-time.json`.
Use a new stem each time. Frozen evidence under `evidence/embedding-v1` must not be
rewritten after delivery. Setup uses `/usr/bin/git` because the host's custom Git
HTTPS helper has an unavailable dynamic library. CMake, Ninja, C++17, Python3,
OpenMP and normal Linux build dependencies must already be present. No package
manager or system changes are performed.

The binary accepts `TEXT.gguf MMPROJ.gguf [threads=6] [--text-only]` and serves
one JSON object per line on stdin, with one JSON object response per line on stdout.
Diagnostics go to stderr. Send EOF to release the model and exit. Requests are
sequential. Query/document operations add the source task prompts; `texts` accepts
already framed application text (BOS/EOS still added by the tokenizer).

```json
{"id":"q1","op":"query","text":"a card transaction was rejected"}
{"id":"d1","op":"document","title":"Payment help","text":"Contact the bank when a credit card is declined."}
{"id":"i1","op":"image","path":"evidence/embedding-v1/inputs/photo.jpg","budget":140}
{"id":"t1","op":"texts","texts":["task: search result | query: payment declined error"]}
{"id":"info","op":"info"}
```

Successful query/document/image requests return `vector` (768 floats); `texts`
returns ordered `vectors`. Every response has `id`, `ok` and request `seconds`.
Images also report original/final dimensions and actual encoder rows. Errors return
`ok:false,error` and no vector. An image error does not prevent the next request.
Invalid/missing model files cause process exit 1. The caller must verify model
hashes before starting; setup does so. The probe itself does not download files.

This ports DigUp's prompts, token truncation (full: 2048, text-only: 512), batch
capacity (16 sequences), mixed-input placement, mean pooling and L2 normalization.
Only CPU is enabled. Media uses mtmd/STB decoding, DigUp's aspect-preserving budget
resize and 22-bit PIL bicubic, followed by mtmd's model preprocessing. Budget
choices are 70,140,280,560,1120. The parity proof covers the source's three image
inputs at 70/140/280. The larger budgets are code paths, not separately validated.

`info.pipeline` deliberately identifies this Linux candidate separately until
independent review accepts compatibility. Model/revision/runtime/pipeline/dtype
and vector dimension must become persisted index identity before integration.
This is an inference foundation, not the release worker protocol: cancellation,
request bounds, root authorization, sandboxing, asynchronous concurrency, version
migration, extraction, restart and UI lifecycle remain engine work. Its image path
argument is intended for explicitly selected files supplied by the parent service.

The copied MIT notices are in NOTICE and DigUp-LICENSE. Model distribution/legal
requirements are handled by the project owner; local weights are ignored by Git.
