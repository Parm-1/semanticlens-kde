# S1 film contract v5 — query and indexing execution

This development contract extends [v4](FILM_S1_CONTRACT_V4.md). Its strict
references, prior opt-in, source authorization and atomic promotion rules remain.
The execution repair is under qualification; this document does not establish
concurrent safety, numerical equivalence or a passing latency result.

## Separate foreground and background contexts

The optional `semanticlens-film-worker` loads one text model and constructs two
independent 2,048-token contexts sequentially. With a CPU budget of at least two,
one thread serves queries and the remaining threads serve one background passage.
A one-thread budget uses serial execution. Contexts, schedulers and workspaces
are separate. Runtime, weights, pooling and normalization stay pinned.

The service accepts paired `--film-worker` and `--film-worker-sha256` options.
Without them, the original serial producer remains available. An invalid optional
worker must leave ordinary file search usable and report the film limitation.
The app supplies a verified adjacent worker when present; its existing default
six-thread budget is unchanged. Two-thread and six-thread configurations require
their own qualification evidence.

The worker's typed begin/poll/cancel protocol permits one bounded background
ticket. The engine remains the sole SQLite writer and returns to its request
queue between polls. Cancellation, disconnect, source revision and epoch checks
still govern promotion. All original title/passage inputs must be encoded; shorter
inputs, partial promotion and query caches cannot substitute for responsiveness.

## Preserve file embeddings and disclose query provenance

Stored file embeddings and their identity remain those of the selected legacy
producer. Document and image embedding still use that producer. Switching between
it and the shared worker must drain or retain the pending passage, unload one
model before loading the other, and resume without losing source coverage.
These transitions can incur cold-load costs.

An ephemeral file-search query may use the new worker only when its exact binary,
CPU budget, model and preprocessing have been qualified against the selected
reference producer. Matching model labels alone do not enable this route. A custom
or incompatible producer keeps its own query path. Legacy queries preserve their
historical prefix and token clipping; film queries strictly reject excess tokens.

Successful search replies carry `query_producer` describing the actual query
executable, hash, protocol, model, preprocessing, context and thread allocation.
This does not relabel stored embeddings. Status retains the last successful
provider, or null before one succeeds, and distinguishes current residency from
configuration. Failed queries and model warmup are not successful-query evidence.

Film representations include the actual execution identity. An older film profile
is not silently relabeled: preserve it for rollback and explicitly reimport into
a separate pilot profile. No user index migration or reset is authorized.

## Qualification and stop boundary

Acceptance requires actual sequential and concurrent vector comparisons, bounded
ticket/lifetime and recovery checks, unchanged legacy index bytes, full passage
coverage, and fresh-query completion during observed background encoding. The
fixed 48 idle and 48 indexing queries retain their one-second p95 criterion.
Measurements require a quiet interval and truthful provider receipts; prior failed
runs remain evidence. Independent review and dedicated KDE interaction checks
are still required. This remains an offline S1 pilot, before cloud enrichment,
real-film quality trials, bulk rollout or a successor release.
