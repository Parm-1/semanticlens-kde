# S1 film integration contract, v2

Linked implementation contract for the parent-adopted [frozen v1](FILM_S1_CONTRACT.md).
The v1 SHA256 remains `2b6d23b4e8ac44ad560c856c389b09b222b3e7e8cd3b9a475af369986bf1ac81`.
This document describes the implemented service candidate, its outcome branches
and bounded source/service controls. It is not independent review or S1 acceptance.
Parent owns native integration and the combined acceptance decision.

## Implemented ownership and producer boundary

`src/core-v5/engine.cpp` creates one FilmStore on the existing Engine worker.
`src/film-v1/store.cpp` owns the separate Qt SQL connection to `film.sqlite`.
No legacy tables or identity fields were added. Film initialization, migration,
status/read errors and background job write failures disable the optional film
namespace or return a film diagnostic; the legacy producer/index stay available.
A failure of the shared producer itself still follows existing global recovery.
Film schema/identity failure preserves the failing database; it does not wipe it.
Legacy `wipe` remains a file-index operation. Film collection removal is explicit.

The helper default executable name is `semanticlens-film-tokenizer`. Service
flags `--film-tokenizer <absolute-path>` and `--film-tokenizer-sha256 <hex>` are
optional. Missing/mismatched helper disables film only. The helper loads the same
text GGUF vocabulary with add_special/parse_special both true, without tensors or
an inference context. Engine checks the binary SHA, model SHA, info identity and
returned exact UTF-8 SHA/integral count. Film identity adds helper/normalization/
formatter/chunker/facet identities to a copy of legacy identity; it never changes
`meta.identity` in index.sqlite.

`Engine::filmCount` counts immutable fully formatted UTF-8, not body length.
The film embedding callback checks <=2048 tokens and bounded serialized JSON,
then sends that exact text through the accepted producer's existing `texts`
operation. Returned vectors use the existing finite, normalized 768 float32 ABI.
Source chunking targets <=640 tokens including title/formatting, splits at exact
Unicode codepoint spans, and optionally repeats a previous <=90-codepoint unit.
The same-unit span map and canonical whole-source join preserve literal retrieval
across passage/unit boundaries. Stored references retain count/formatted SHA.
No tokenizer estimates, probe modification or different producer artifact is used.

`engine/CMakeLists.txt` embeds the two parent schema JSON files as Qt resources;
runtime imports need no source-tree paths. FilmSchema implements strict object
properties, union types (including explicit null), integer versus number,
required/enum/const/ref/anyOf/allOf/if/then/contains and declared bounds. Adapter
v3 remains worker-owned; the manifest supplies source_kind. Adapter source_format
is srt/vtt/text/markdown/pdf_text, normalization semanticlens-film-units/1.
PDF extraction runs pdftotext on the owned snapshot, with bounded output/time.
No OCR or finished-film observations are claimed. Timestamp integers must be
exactly representable through 2^53-1 milliseconds; evidence offsets use actual
canonical codepoint length before integer conversion.

## Protocol and native operations

All operations share protocol1 `{id,op,...}` requests and `{protocol:1,id,ok,...}`
responses. IDs are socket-owned, distinct from legacy UI IDs. Unknown operation
fields reject. Errors carry error/error_code when produced by FilmStore; late
cancellation can carry `ok:false,error:"canceled"` without error_code. Film
revision and epoch are decimal strings. Revision changes committed film state;
epoch also invalidates disclosure on watcher observations. Neither is a legacy
media membership token. Full original operation/response examples are retained
in the service runner records, with actual dynamic values.

| Operation | Request fields after id/op | Successful payload |
|---|---|---|
| film_capabilities | none | capability film_v1, ready, schema_version1, formats, modes, normalization_version, ranking_version, embedding_identity, film_revision, film_epoch, limits |
| film_status | optional job_id | status {ready,model_ready,busy,paused,phase,error,film_revision,film_epoch,collections,counts,jobs,warnings,coverage}; direct no-job status also has watch_error |
| film_add_collection | path, optional exclusions[] | accepted, collection_id, film_revision, film_epoch |
| film_sidecar_digest | collection_id, relative_path | sha256, film_revision, film_epoch |
| film_import_manifest / film_import_analysis | collection_id, relative_path, expected_sha256 | accepted, job_id, job_state pending, film_revision, film_epoch |
| film_cancel_job | job_id | accepted, job_state, film_revision, film_epoch |
| film_reconcile | optional collection_id | accepted, job_id, job_state pending, film_revision, film_epoch |
| film_set_exclusions | collection_id, exclusions[] | accepted, removed, film_revision, film_epoch |
| film_remove_collection | collection_id | accepted, removed, film_revision, film_epoch |
| film_remove_analysis | collection_id, film_id, edition_id, analysis_id | accepted, removed, film_revision, film_epoch |
| film_search | text, mode source/hybrid; optional collection_id/null, filters{language,edition_id,source_kind}, limit1..50, spoiler_policy | results[], film_revision, film_epoch, ranking_version, embedding_identity, limited, warnings |
| film_evidence | reference, expected_film_revision, optional spoiler_policy | evidence, film_revision, film_epoch |
| film_navigation | reference, expected_film_revision, action open/reveal, spoiler_policy allow | navigation{token,snapshot_path,page,action,source_revision,film_revision,film_epoch,label}, film_revision, film_epoch |
| film_release_navigation | token | released, film_revision, film_epoch |

`film_sidecar_digest` is the adopted additive extension. It admits and reauthorizes
an under-root JSON sidecar through the same held-descriptor lease as imports,
returns a digest only, activates no rows, and returns no content. The native
importer must use it instead of a permissive GUI QFile reader, then send the
returned expected_sha256 to import. Hash mismatch at import still rejects.
Exclusions are literal relative path prefixes; spaces are meaningful.

Status counts include editions/sources/active analyses/vectors/units/claims/
passages/errors. Jobs expose job_id/kind/state/stage/done/total/error; aggregate
status also includes per-job error_code and warnings. Terminal states are
complete/failed/canceled/interrupted. stage progresses admission/snapshot/parse/chunk/
embed/reauthorize/committed. Recovery marks unfinished durable jobs interrupted,
removes owned extraction directories, and requires explicit reimport. Identical
manifest/analysis bytes are idempotent; conflicting analysis ID bytes reject;
a new same-edition analysis with supersedes atomically deactivates its predecessor.

A row includes collection_id, film_id, edition_id, display_title, year,
film_revision/film_epoch, source_exact, identity_match, suggestion, score,
ranking_version, lanes[{lane,rank,score,reference}], evidence[] and warnings.
Source exact is only literal canonical source content; analysis literals have
analysis_lexical, not source_exact. Each edition/lane contributes its best hit,
not extra votes for repeated passages/claims. Rank fusion is RRF60 with curated
weight0.5. Exact source and identity tiers precede suggestions. Aesthetic claims
are retained in imported metadata but excluded from retrieval in this pilot.
The service returns ranked suggestions for no-answer queries and explicitly
warns that this is not a calibrated absence test. Hidden text does not make
ranking spoiler-blind.

## Analysis-only references and default disclosure

Source references retain reference_kind source, collection_id/film_id/edition_id,
source_id/source_revision/unit_id/start_codepoint/end_codepoint and nullable
analysis_id/claim_id. Anchored analysis references must match a stored supporting
anchor; the service never borrows another quote to explain that claim.

Unverified priors legitimately have no anchors. Their adopted extension is:

```json
{"reference_kind":"analysis_only","collection_id":"...","film_id":"...",
 "edition_id":"...","analysis_id":"...","claim_id":"..."}
```

film_evidence accepts that revision-bound shape, reauthorizes the analysis
sidecar and **every** declared input source, and returns analysis_text only when
allow is explicit. source_excerpt/unit_text remain null, label is `Unverified
prior; no source anchor`, and navigation rejects it. Invented source/offset fields
reject. No nearest-source association is fabricated.

Default spoiler policy hide redacts source_excerpt, unit_text, analysis_text,
limitations and not_observable narrative arrays in both search and evidence.
Title/reference/provenance metadata remain visible. Explicit allow reveals bounded
source/unit text (1000 codepoints, with limited flag) and imported analysis text.
exclude_spoilers removes spoiler-bearing claims from retrieval. It is not an
assurance about source passages or other semantic ranking signals.

Analysis evidence exposes origin plus the exact declared generation object
(provider/model_id/model_revision/prompt_sha256/settings), created_at, basis,
facet, scope, input_sources coverage, limitations and not_observable. It always
labels semantic_review unreviewed. Valid anchors verify source association/quote,
not the truth of an imported interpretation, model declaration or coverage intent.

## Promotion, disclosure phases and cleanup

Import admission is acknowledged before incremental work. Jobs remain owned by
that socket while it stays connected, including when the native Files tab is
shown. Disconnect cancels acknowledged unfinished jobs; it is not mere request-ID
cancellation. A valid film_cancel_job envelope records intent at Engine ingress
under the mutex, before its handler runs. Wrong-owner, wrong raw job type or
unknown fields cannot record that intent. Once recorded, cancel intent is not
undone by canceling the cancel envelope. This exception to request-envelope
cancellation is explicit.

`FilmStore::promote` reauthorizes all held leases, then `callbacks.promote` holds
the same Engine mutex used by ingress cancel, disconnect and watcher epoch bump.
It checks stopping/paused, live owner, registered job owner, cancel intent and
captured epoch, executes the SQLite transaction and commit, then retires owner
registration. ownerAlive is not called while holding that mutex. Admission wins
before commit => no promotion; transaction admission wins => later cancel is
false and committed state remains. The ordinary observer cannot claim cancel
won just because bytes reached the socket while that mutex was held.

Film jobs do not call servicePending recursively. The existing legacy indexing
calls servicePending from scan/index phases outside a Task handler; no outer film
Task frame exists there. Disclosure validity closes over its explicit Task,
background validity over a weak job pointer. There is no global-current-task
inheritance contract. Worker SQL/storage stays on one thread.

Disclosure Task filmEpoch is captured at ingress and checked by validLocked for
search/evidence/navigation/digest. Source reads and reauthorization precede
response construction, then the worker attaches the captured epoch. The queued
service reply compares it against currentFilmEpoch before writing. It must not
stamp previously checked text with a new epoch. Watcher callbacks increment that
same epoch; direct status can report current epoch while worker reauthorization
is paused. Revision rechecks/held inode+hash checks apply as well. Before file
watch registration or after a declared watch failure, there is no watcher
observation proof; watch_complete describes the last watcher synchronization, not an
instantaneous registration barrier, and every disclosure still checks its lease. In-place edits after the final check remain the usual external
filesystem race; navigation uses the verified owned copy, not an immutable claim
about opening the original path.

Navigation is an opaque issued token and a protected0400 source copy inside an
owned0700 temporary directory. Parent UI handles external open/reveal after
receiving this copy; the service launches no desktop application. At most one
copy per issuing socket and sixteen total. Explicit release, replacement
navigation, revision change, disconnect and shutdown clean copies. A discarded
late navigation result queues owner-bound release; errors after issuance remove
the copy. Startup removes interrupted owned extraction directories. Original
source paths and bytes remain untouched.

## Bounded observations and stop rule

Frozen service evidence is under `local/film-store-v1`; the delivery report lists
original runner/results paths, exact candidates and controls rather than merging
all booleans into one acceptance count. Base evaluation uses the parent's frozen
`local/film-fixtures-v1/corpus`. Synthetic malformed/rank/time controls are separate
copies. v1 contract/token control receipts remain unchanged. Actual accepted rc2
probe SHA is fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0.

Successful source/service controls permit parent native consumption and
independent combined review. Failure branches preserve the failed attempt and
use a linked successor for repairs. Stop at frozen delivery and independent
review/native acceptance; do not claim S1 acceptance, run S2/S3, mutate installed
rc2, release/merge/publish, add providers or read personal corpus. Full native
interaction, broad legacy multimodal regressions, physical disk exhaustion,
crash exactly inside filesystem/SQLite commit, peak RSS and general ranking
quality remain outside this bounded service proof unless explicitly supplied
by the parent or successor controls.
