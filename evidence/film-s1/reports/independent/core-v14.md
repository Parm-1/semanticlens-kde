# Independent affected review — private qualified-service branch

No new blocker found in the bounded five-case service provenance/routing milestone. This supports the gate-enabled branch at the recorded service-reply and immediate status phases. It does not grant delivery routing, production use or whole-S1 acceptance. Parent remains the acceptance and integration consumer; the original core author owns subsequent runtime qualification.

Actual trigger: automatic active S1 continuation, commissioned by parent after accepted v13. Prior requests are answered. This reviewer is uninvolved in candidate implementation. Scope is source/raw only; writes are confined to this linked v14 directory. Earlier reports, candidate, raw evidence and canonical state remain unchanged. Dedicated desktop, offline scope and installed-rc2 preservation remain in force.

## Exact candidate and affected delta

- Service: `a59a737bb7e27bcd593d237e36ab113742f566dadd25418d316f1ffebb28f6e3`.
- Worker: `dff1212b64fcfb176ce4a3666bd5e6b825259ad3b46f04a625f3995a9dd836e5`; full producer: `fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0`; tokenizer: `bca52b5f2945f2959b7493f3cd6d48f0cbce35547d6e215b95b555f643659a1b`.
- Frozen engine: `cbd039798cddd6292672d58685ec5ee3c608784def8cc5136ecc260dbd4aa21e`, byte-identical to the accepted provenance repair. Worker, service main, store and engine-header copies match the preceding frozen source. Only the qualification header changes the accepted provenance behavior: hash above, budget exactly 2 or 6. Header SHA256: `49b64e0d14465a99850df4b6594d597b3ee0076ea9f513a68e8afecb63d40119`.
- Adopted Contract V7: `839fae46145efa996b3fb7c0ed23b54bb2b5f81f39e509bcf11f9f332df4a1c7`. Milestone report: `e3cb00927d1150bab81e5b45c3a47e135c94c0b1cf43e1abc629819010dd31ca`; packet manifest: `b4712f7af1a6cfdca7ad74d5b8e6b664097eff9ab38206e31a84231858fc3d67`.

Frozen `private-candidate-v1/source/src/core-v5/engine.cpp:1083–1113` separates math qualification from ordinary-file compatibility. The former requires exact qualified hash, budget and validated worker info; the latter additionally requires accepted full-fa selection. `ensureFilmWorker` validates selected executable/model hashes and strict info fields before use. `queryVector:1191–1196` assigns provider only after a valid response passes `vectorBlob` and task validity. `vectorBlob:130` checks 768 numeric, finite float values and norm. `recordQueryProvider` is reached after successful search; warmup/info cannot create a successful query provider. These are source applicability checks, not a replay of worker math or observed thread allocation.

## Primary readbacks and contradictory cases

The independent `read_service.py` parses all 567 original request/reply records, decodes each retained UTF-8 hex reply and checks equality with parsed JSON, exact request/reply IDs and sequential clocks. `records.jsonl` and `records.json` are identical. It does not use the author’s outcome flags to establish case behavior.

| Case | Actual query IDs: initial file / film / final file | Film provider | File provider | Immediate film status ID |
| --- | --- | --- | --- | --- |
| Qualified budget 2 | `97 / 114 / 116` | `shared_worker`, exact dff, strict film preprocessing | `shared_worker`, legacy clip preprocessing | `115` |
| Qualified budget 6 | `80 / 92 / 94` | same, budget 6 | same | `93` |
| Unknown worker, budget 2 | `95 / 116 / 118` | `selected_worker`, unqualified, null math | `accepted_legacy`, full fa | `117` |
| Exact dff, budget 1 | `106 / 124 / 126` | `selected_worker`, unqualified, null math | `accepted_legacy`, full fa | `125` |
| Qualified budget 2, custom legacy | `87 / 106 / 108` | `shared_worker`, exact dff | `selected_legacy`, unqualified, null math | `107` |

IDs have the case-name prefix shown in `service-readback.json`. All fifteen queries have successful original replies and actual owned-fixture results. For each film result, the Harbor lexical reference selects line 3, codepoints 6–49; the frozen source bytes hash to the emitted revision and that span equals the actual query. All file results identify the case’s `legacy.txt` containing `LEGACY-CLOCK-481`. Vector lanes accompany the film literal result; this is not merely a metadata-only request.

The two worker negatives vary actual executable hash and actual configured budget separately. Both retain matching self-declared model labels, yet all fourteen unverified math fields and reference-producer identity are explicit JSON null. Declared info and configured model hash/budget are separate objects. The custom-legacy negative holds exact dff and qualified budget constant: film remains qualified, while initial and final ordinary-file queries retain custom selected-producer path/hash, `query` command and null math. It therefore exercises the extra full-fa guard rather than weakening worker math qualification.

Known provenance’s hash, budget, preprocessing, model/revision, model hash, runtime, context/batch/ubatch sizes, sequences, pooling, dimension and allocation labels match the frozen engine/header and previously accepted worker scope. This milestone does not independently observe allocation or inner numerical work. Custom proxies forward real producers with altered query formatting; inner arrays and numerical effects of that alteration are not retained or qualified here.

Every ready snapshot before the first query has null provider and `no_successful_query`. Each next request after the film query is the listed status request; its provider equals the preceding reply and state is `last_success_resident`. Unknown workers follow the worker process, not the legacy process. The custom-legacy case reports `legacy_queries_qualified:false` despite known film provenance. This proves these immediate film-status associations only, not later idle transitions or post-fallback residence.

## Stored identity, launch failure and closure

For all five cases, retained pre/post-import identity rows preserve raw type and complete base64 value, and segment-vector bytes match. Independently opening each final SQLite database with `mode=ro&immutable=1` reproduces those exact rows and the full 3072-byte vector. Each stored identity’s producer hash matches the selected legacy executable, including the custom artifact. The vector SHA256 is `662f2ddfa9c3fc195fb146bad6c100a00fd6e6832790c5db14b9157de058f0bc` in each case. This compares the stated snapshots and final database; it does not prove every intermediate write or a complete lifecycle.

Attempt 77066 has no completed cases. Its original service log says bwrap exec permission denied; original closure is exit 1, unforced, empty group. Sealed candidate binaries remain 0644. Linked runnable binaries are 0755 with identical four hashes; no model failure is inferred. Corrected 18392 launch receipts contain bwrap argv, private output binding, clear environment/network namespace arguments, PID and monotonic start/end. Each actual service-reply sequence lies inside its launch interval. All five terminal receipts are exit 0, unforced, empty process groups, matching launch PIDs. These are runner/namespace-group closure observations, not a generalized process-lifecycle proof.

## Limits, references and handback

No demonstrated defect or new required missing evidence within this bounded milestone. Reuse of accepted worker numerical/context/abort evidence is justified by unchanged dff bytes and exact accepted budgets; the 146-vector workload was not repeated. Full 512-window/last-unit coverage, indexing handover, drain/retain, owner/pause/cancel/fault/restart/profile/rollback, realized allocation, quiet timing, current native/legacy integration and whole-S1 acceptance remain separate obligations. A configured capability label is not an allocation witness.

Author’s retained functional wall interval is 57.813625766997575 seconds for the five-case runner, including setup and teardown. It is not query latency or quiet p95. Model/tool tokens, billing and cost are unknown; reviewer total elapsed time is not measured. No model/tool handoff was inserted into a measured workload by this reviewer.

`REFERENCES.sha256` binds 195 unique original packet, source, binary, contract and raw references, including all 170 packet references. `service-readback.json` retains the independent category/field comparisons. `SHA256SUMS` binds this report, helper and readback/reference files. No service, model, debugger, compiler, native app, network client or viewer was launched. All reviewer read/check commands exited; all SQLite connections are closed, no live reviewer process handle remains. Author retains exclusive backend runtime; this handback does not release its ownership. Next consumer is parent for bounded scope acceptance and original author for subsequent qualification.
