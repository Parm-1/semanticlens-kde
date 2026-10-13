# S1 film contract v7 — selected-worker provenance

Linked successor to [v6](FILM_S1_CONTRACT_V6.md), SHA256
`5139b84a6da820005679f9449bc107936fa0572e7af1ca3cb0071e935b592d26`.
Earlier contracts and evidence remain unchanged. This records the adopted
service repair; affected runtime validation and final S1 acceptance remain pending.

A worker's compatible `info` reply establishes protocol compatibility, not its
mathematical implementation. Known shared-worker provenance requires the exact
qualified executable hash, an independently qualified configured CPU budget and
validated protocol information. Film queries use that qualification independently
of the selected legacy producer. Routing legacy queries through the shared worker
additionally requires the accepted legacy producer identity.

Other successful worker queries report `mode: selected_worker` and
`qualification: unqualified`. Retain the actual executable path/hash, protocol,
command, validated 768-dimensional vector ABI and ephemeral query disposition.
Keep self-reported `declared_info` and the configured model hash/CPU budget
separate from observed computation. Model, revision, runtime, preprocessing,
context/batch sizes, pooling, thread allocation and reference-producer claims
remain null when unqualified. Matching self-reported labels cannot grant them.

Residence status follows the process that produced the successful query for both
`shared_worker` and `selected_worker`; it must not mistake the selected worker
for the legacy process. Existing `selected_legacy` behavior and stored file-vector
identity remain intact.

The repair does not change the embedding worker or relax byte equivalence,
source coverage, lifecycle, privacy or the fixed latency criterion. A private
candidate may enable the exact qualified worker/budget combination for service
validation after affected numerical and execution review. Delivery routing
acceptance still requires service integration evidence and independent review.
