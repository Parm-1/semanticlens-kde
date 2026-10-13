# S1 film contract v6 — cancellation, scheduling and provenance

Linked development successor to [v5](FILM_S1_CONTRACT_V5.md), SHA256
`6014301b8fc0a747135b3ad6f8147c216d86d355c8b189cde637be524d6c786c`.
These repairs address independent source-review findings. Runtime qualification
and acceptance remain pending; earlier evidence is not relabeled.

An accepted worker cancellation must prevent a later successful vector poll for
that ticket. Cancellation and final publication share one synchronization point.
The engine's separate canceled-job promotion guard does not establish this worker
protocol property. A cancellation/reuse check requires an actually accepted cancel
and a terminal canceled result without a vector; an unsuccessful cancel followed
by a vector cannot qualify the abort branch.

A new begin rejects while the current ticket is active or completed but unread.
Only a terminal poll permits its replacement. Repeated terminal polls use the
same bounded slot; cancellation revokes future successful polls, and an old
ticket cannot cancel its replacement. A pending poll with no intervening terminal
poll therefore keeps a second-begin rejection applicable even if encoding finishes
before the second request. This proves slot disposition, not concurrent math.

When a film job and an eligible ordinary file scan both have work, the engine
alternates one film step and one legacy scan step at its existing worker boundary.
File indexing must make progress before the film import finishes. Switching to the
legacy producer drains or retains the complete pending passage, preserves its
owner and request context, and unloads one model before loading the other.
Full source coverage and control handling remain required. Cold handovers are
reported separately; the film-only timing matrix does not qualify every mixed
file/image workload.

The exact qualified reference producer may retain `accepted_legacy` provenance.
An unfamiliar selected producer uses `selected_legacy` with
`qualification: "unqualified"`. Its actual executable/hash, observed command
protocol, command, validated vector dimension and ephemeral-query status remain
available. Model, runtime, preprocessing, context, pooling and actual thread
allocation fields are null. `declared_info` contains the producer's own info
reply; `configured` records the supplied CPU budget and verified model-argument
hashes. Neither is proof that the producer used those settings. Stored file
identity, selected-producer behavior and fallback remain unchanged.

Clock evidence must reconstruct fragments using their process, descriptor,
direction and pipe generation. Changed pipes cannot inherit unfinished bytes or
timestamps. Unfinished relevant streams at the final observation boundary
invalidate a completed causal record. Transport pending states alone remain
insufficient to prove simultaneous model execution.
