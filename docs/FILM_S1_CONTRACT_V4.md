# S1 film contract v4 — strict evidence references

Linked successor to immutable [v3](FILM_S1_CONTRACT_V3.md), SHA256
`b586d5e0235affc4fe9f3417ac88401b5c0b4de0b39ee12d8e56e785123f170e`.
The original same author repaired independent review v5 F6 and qualified F3–F5
in the disposable backend package `local/film-store-v3`. Parent owns acceptance.

All valid emitted operation, response, capability, ranking and database shapes
remain unchanged. No UI capability change or reimport is required. The guide can
link this clarification. Prior opt-in remains the explicit typed hybrid-only
boolean described in v3; Reveal never enables prior search participation.

`film_evidence` and `film_navigation` validate a reference before lookup or text
disclosure. Both `analysis_id` and `claim_id` must be explicitly present. They
must either both be JSON null, or both be nonempty strings matching
`^[A-Za-z0-9][A-Za-z0-9:._-]*$`, at most 128 characters. Wrong raw types, missing
fields, empty identifiers and mixed null/string pairs reject `invalid_request`.

A pure-source reference uses `reference_kind: "source"` and the paired nulls.
It requires its collection/film/edition, source identifier/revision, canonical
unit and integral codepoint offsets. Before lookup, offsets must satisfy
`0 <= start < end <= INT_MAX`; after resolving the authorized stored unit,
`end <= codepoint_length(unit_text)` is required. No empty excerpt is valid.
Collection, film, edition, source revision, expected film revision, captured
epoch and source reauthorization checks still apply.

A grounded analysis reference uses `reference_kind: "source"` and the paired
string identifiers. The claim must be active in that same edition and the exact
source/unit/span must be one of its stored supporting anchors. A prior-only
reference uses `reference_kind: "analysis_only"`, paired string identifiers,
and none of the source/unit/span keys. It must resolve an active
`unverified_prior` claim, reauthorize all declared inputs, and obey hide/allow.
It carries no source excerpt and cannot navigate. No nearest quote is borrowed.

The service error remains the existing `{ok:false,error_code,error}` envelope.
No manager schema, provider path, producer ABI or legacy identity changed.
Actual malformed pre-repair and repaired positive/negative requests, boundary
controls, phase witnesses and explicit limits are in the linked author report.
This clarification is an implementation contract, not independent acceptance.
