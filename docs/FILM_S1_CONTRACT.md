# S1 film integration contract, v1

This is a bounded architecture investigation for the explicitly authorized offline
S1 pilot. The parent owns adoption, implementation and acceptance. It is not an
implementation acceptance or independent review. Starting internal HEAD:
`9edcbc1cbfbbf55533fcc6117da896ead4e98bcd`, branch `codex/film-s1`. Read-only
production inspection; owned controls and frozen source copies are under
`local/film-contract-v1`. Parent has adopted separate `film.sqlite` on the existing
Engine worker, server-default spoiler hiding and authorized owned-copy navigation.
Remaining new semantics below are recommendations for that owner.

## Primary evidence and qualified outcome

The build selectors are root CMake, `engine/CMakeLists.txt` and `probe/CMakeLists.txt`.
Selected production is core-v5/service-v3/ui-v4/runner-v5, not historical siblings.
Frozen inspected bytes and hashes are in `source/` and `source-pins.json` beneath
the owned control directory. References below refer to those frozen bytes; later
implementation must use successor evidence rather than rewrite this report.

* `probe/main.cpp:48-65,70-73,94-103`: the service starts the multimodal CPU
  producer, effective limit 2048. Text-only mode has limit512 but is not the
  service mode. Tokenization is UTF-8 byte length with `add_special=true` and
  `parse_special=true`; excess tokens are silently resized and the final token
  replaced by EOS. Query is `task: search result | query: ` + text. Document is
  `title: ` + (empty title ? `none` : title) + ` | text: ` + text. `texts` consumes
  already formatted strings without another prefix.
* `vendor/llama.cpp/include/llama.h:351,1266-1281` and
  `src/llama.cpp:344-367`: `vocab_only` loads the actual vocabulary then returns
  before tensors. Negative tokenize results mean required capacity; INT32_MIN
  means overflow. `src/llama-model.cpp:1327-1331` skips non-vocabulary hparams, so
  vocabulary-only context/dimension accessors are not the producer limit oracle.
* `core-v5/engine.cpp:373-431,450-497`: model hashes, producer hash and complete
  compact identity JSON are checked. Existing tables require exact identity.
  Identity includes producer SHA, representation3, extractor versions and engine
  pipeline. Adding fields to it, changing probe bytes or switching producer mode
  would reject the legacy DB. No such change is recommended.
* `core-v5/engine.cpp:231-282,529-540,701-884`: ingress, single worker,
  cancellation, nested `servicePending`, snapshot, extraction, vector staging and
  commit are distinct phases. `servicePending` sets/clears the one global active
  task, so nested film handlers must not inherit that as a lifetime contract.
* `core-v5/engine.cpp:80-135,570-602,709-737,859-879`: local mount admission,
  held descriptors, same inode/current hash promotion check and selected roots
  are actual protections. Legacy `eligible` admits only txt/md/pdf/jpg/jpeg/png;
  calling it for SRT/VTT/JSON would incorrectly deny film inputs.
* `service-v3/main.cpp:61-67,145-219`: one data-directory lock; socket owner UUID;
  protocol1 envelope; aggregate buffered ingress65536 bytes; max64 outstanding
  requests; IDs1..128 characters; queued reply delivery. Existing late generation
  suppression applies only to `search`. New film operations need an explicit gate.
* `ui-v4/backend.cpp:228-290,325-349`: request IDs and status revisions suppress
  old file results. Media requires complete membership in the current legacy
  result list and legacy roots. Film rows cannot be passed to `previewResult`.

Owned controls, not a film implementation: 13 vocabulary-only/full-load token
arrays were identical, including UTF-8/supplementary codepoints, empty title,
special-token spellings and exact511/512/513/2047/2048/2049 lengths. Raw counting
without special handling yields different counts (see original token replies).
Three actual rc2 producer query/document vs exactly formatted `texts` pairs had
identical768-element vectors (maximum absolute difference0). The actual producer
reported2048/768. Receipts: `token-requests.jsonl`, `token-replies.jsonl`,
`probe-requests.jsonl`, `probe-replies.jsonl`, `results.json`; compile/run logs are
retained. Helper links the pinned accepted `build-probe` archives; the vendor HEAD
is `4d756bc72bf00a4aacf410ae15a2d315f3db400d` with the existing modified
llama-context.cpp recorded and copied. No runtime was rebuilt or modified.

These controls qualify the companion seam at the inspected runtime/model, not
the future strict wrapper, importer, scheduler, film ranking, UI or broad quality.
The direct rc2-stage executable was an owned subprocess with owned stdin/stdout,
not the installed user service. No socket, index or personal source was touched.

## Producer integration

Add a sibling tokenizer executable from the same runtime and text GGUF. Use
`llama_model_default_params`, `n_gpu_layers=0`, `vocab_only=true`, no context,
media projector, embedding warmup or tensor load. The production executable
only needs vocabulary; the disposable control loaded full weights to compare.
Leave `embedding-probe` and its `info` untouched; do not make its link depend on
the new helper. Pin model/runtime/helper digests separately in film identity.
Reuse the exact accepted probe artifact for existing DBs, or demonstrate byte
identity after a build; unchanged source alone is insufficient. The owned
rc2-stage probe hash is `fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0`,
whereas the accepted standalone build-probe artifact has hash
`f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf`.
Do not substitute one for the other in a DB identity tied to the first artifact.

Companion JSON-lines protocol, bounded64KiB input and output:

```json
{"id":"count-1","op":"count","text":"title: none | text: Harbor"}
{"id":"count-1","ok":true,"token_count":8,"utf8_sha256":"<digest-of-exact-text-bytes>","tokenizer":"llama.cpp-b11461+no-logits;special=true,true"}
```

The numeric8 is illustrative, not a frozen expected count. Do not report tokens
or use client-supplied counts in production. Unknown fields/ops, invalid UTF-8,
NUL, oversized bytes and tokenize overflow fail closed. Reply ID, integral
nonnegative count and exact input digest must match. Catch negative required
capacity safely before negating INT32_MIN. Keep companion lifetime at most the
existing producer's idle lifetime (10 seconds) and close both on shutdown.

Film embedding wrapper renders the exact document/query string once, counts
that immutable QByteArray, requires count<=current verified producer info2048,
then submits the very same string through existing `embed({op:"texts",texts:[s]})`.
Validate its one returned vector with the existing768/finite/normalized float32
ABI. Alternatively `document`/`query` are safe only if all fields are retained
unchanged after the count. Never count raw body, UTF-16 length or an estimated
prefix. The strict wrapper rejects an overlength query; passage builder splits
long units at Unicode codepoint boundaries with reversible spans. Titles are
bounded and included in counting; reject a title/prefix that leaves no budget.
Target640 tokens including formatting is an S1 default, not a quality result.
Persist exact formatted SHA/count plus tokenizer/formatter/chunker identity.
Also enforce fully serialized producer/companion request limits before writing;
the token limit does not imply a byte-size limit for repeated whitespace or JSON
escapes. Split/reject oversized formatted strings rather than truncate bytes.

Before accepting the new wrapper, send actual2047/2048 positives and2049
negative, as well as short-body/long-title and embedded-special-token cases.
Observe that the negative makes **no producer request**; green wrapper flags
alone do not establish no truncation. A helper mismatch disables film inference
only. Failure of the shared producer itself retains existing global recovery.

## Storage, isolation and unit contract

One Engine thread owns both Qt SQL connections. `film.sqlite` is private beside
`index.sqlite`; use a distinct connection name and a separate schema/identity.
No second service/model or GUI DB access. Transactional v1 creation/migration,
foreign keys, WAL and FULL synchronous; reject a newer/unknown schema without
wiping it. A film migration failure sets `film_status.ready=false,error=...` and
leaves legacy `operational`, `meta.identity`, roots, revision and rows alone.
The existing `wipe` remains the file-index operation. Film collection removal is
explicit; the UI must name these scopes accurately.

Minimum relations: collection roots/exclusions and consent epoch; explicit
work/edition/source association; source revision/normalization and canonical
units; token-counted source passages/vectors; analyses/claims/anchors; separate
source lexical, analysis lexical and curated lanes; jobs; active pointers and
film meta. Composite source keys include collection/work/edition/source/revision;
an anchor cannot resolve through a globally coincident source_id. Analysis/claim
IDs have same-edition uniqueness; identical bytes are idempotent, conflicting ID
reuse rejected, explicit replacement uses new analysis_id and `supersedes`.

Raw source revision is SHA256 of exact snapshotted bytes, not normalized text.
The commissioned QtCore adapter contract is `FilmAdapters::parse(QByteArray,
QString)` -> QJsonArray of units. Its normalization is
`semanticlens-film-units/1`; strict UTF-8, CRLF mapping, no Unicode folding.
IDs: `cue-{ordinal1}` for subtitle cues (supplied ID is `original_id`),
`line-{physical}` for text/Markdown, `page-{page}-line-{physical}` for extracted
PDF text. Consumer fields: unit_id/text/kind plus page, cue_start_ms/cue_end_ms,
original_id and reversible raw mapping when applicable. Absent locators are
null, not invented0. Source_kind (screenplay/subtitle/synopsis/reference/
curated_note), language and edition_relation belong to the manifest, not guessed
from text. PDF extraction runs on the owned byte snapshot; record extractor
identity separately. Empty/nonextractable pages have explicit coverage warnings;
S1 does not silently claim OCR or finished-film observations.

Evidence offsets are zero-based Unicode codepoints, end exclusive, into one
canonical unit. Convert to UTF-16 before QString::mid; reject unpaired surrogates,
bad ranges and unequal quotes. Whole-source literal search uses a persisted
canonical unit join plus a reversible join-offset map, so phrases crossing
retrieval windows and unit boundaries have stable source references. The join
separator and markup/newline policy are versioned. Literal matching must not
borrow analysis text or a work title to assert source_exact.

## Source lease and lifecycle hooks

Share the actual local admission implementation, not legacy `eligible`.
Film policy checks collection selection/exclusions, hidden/denied paths, clean
UTF-8 path, no traversal/symlink/magic link/provider crossing, owned data-dir
exclusion and regular file type; then `localPath` obtains the descriptor.
The UI/path request itself is not proof of admission. Keep JSON manifests,
analysis files and referenced sources under the selected collection.

Recommended worker-only callbacks:

* `snapshot(collection_id, relative_path, expected_sha256, cancellation)` ->
  source lease containing held descriptor, initial dev/inode/stat, copied owned
  path/bytes, actual SHA256, collection epoch. Copy/hash in64KiB units with bounds.
* `reauthorize(lease, cancellation)` -> reopen **current** lexical source with
  local policy; compare dev/inode/regular-file and current held SHA to snapshot;
  recheck collection epoch and film cancellation immediately before promotion.
* `strictEmbed(formatted_bytes, cancellation)` -> count gate followed by existing
  embed transport, response/vector checks; cancellation after the reply must
  prevent promotion. No nested SQL transaction across embedding or extraction.
* `filmWatchPaths` joins collection roots, manifest/sidecar and active source
  watch targets into the existing watcher. Watch changes advance an atomic film
  epoch at service ingress and enqueue bounded reconcile. `film_reconcile` is
  also explicit for watcher-capacity/missed-event recovery.

Reconcile inspects registered assets only, never discovers the personal home.
For a changed/missing/excluded source, withdraw its passages and every dependent
multi-source analysis in one film transaction, bump film revision, clear caches
and record a repair warning. Reimport a manifest with new declared digests to
restore source-only results. Never auto-reattach a rename across edition/root;
an unchanged content hash still needs explicit newly authorized association.
An analysis update failure leaves the valid source and prior valid analysis
active with a failed-update diagnostic. Removal deletes owned staged/private
content as promised; sanitized job failure metadata may remain. Secure forensic
erasure from WAL/backups is not claimed.

Freshness is an observation-phase guarantee: reauthorize candidates before
search/evidence disclosure and leases before promotion; serialization rejects
a changed film epoch. Native watches are asynchronous and are not proof that
every filesystem mutation has already been observed. No unlocked path can be
promised permanently unchanged after the reauthorization point.

## Scheduling, job ownership and recovery

Maintain separate atomic film epoch from legacy generation. Film mutations do
not increment legacy index_revision or cancel legacy queries. Shared pause/resume
pauses/resumes film building too; legacy pause/generation behavior is preserved.
Shutdown cancels jobs, closes companion and shared producer through Engine
destruction, and joins the worker. A film-only parse/validation/SQL failure never
calls `Engine::setFailure` unless the shared producer has actually failed.

Accepted imports create durable jobs, then reply; ack is not completed import.
Store request/session owner separately from durable job ID. `filmStep()` runs
outside `handle/servicePending`, after interactive queued requests and before
the next wait, one bounded snapshot/parser/passsage/vector/commit unit at a time.
Do not recursively call `servicePending` from a film task. If refactoring legacy
reentrancy, restore the prior active-owner frame and test cancellation during
nested controls; do not merely save its ID. No transaction or live QSqlQuery
iterator may cross a yielded step. Stage job content/vectors privately; promote
all active pointers and one revision atomically only after reauthorization.

`cancel` retains existing outstanding-envelope semantics, including film_search
and film_evidence. A durable acknowledged job uses `film_cancel_job`; it accepts
only the initiating live socket owner while job is pending/running. Disconnect
cancels that session's uncommitted jobs. An active promoted job cannot be canceled
retroactively. On startup mark nonterminal jobs interrupted, discard incomplete
staging and require explicit same-reference reimport; deduplicate against valid
committed digest/identity. This is idempotent recovery, not a false automatic
resume guarantee. Reimport may link its previous interrupted job in diagnostics.

Status remains a locked snapshot served directly on the service event thread.
The GUI stays asynchronous; an in-flight legacy embed may still delay other
worker tasks up to the existing90s transport bound. Cancellation marks intent
immediately but never kills the shared producer while it owns a request. One
embedding per background step and interactive queue priority establish bounded
interleaving, not a hard query-latency guarantee.

## Complete proposed protocol1 film IPC

All requests carry existing string `id` and `op`; all replies carry protocol1,
same id and boolean ok. Reject unknown fields, wrong JSON types/enums and invalid
integral bounds. Error adds string error_code/error; no content/path leaks on
failed admission. Codes: invalid_request, unsupported_schema, film_unavailable,
not_authorized, stale_revision, source_changed, token_limit, limit_exceeded,
conflicting_id, canceled, interrupted. These additions do not change legacy
request/error shapes. `film_revision` and `film_epoch` are opaque decimal strings
(avoid JSON-double precision); source_revision is lowercase64-digit SHA256.

Recommended explicit S1 limits, to freeze before fixtures/measurement:8
collections,128 editions,32 sources/edition,32MiB/source,2MiB UTF-8 extracted text,
10000 units/source,1024 passages/source,8192 total active vectors,64 claims/analysis,
16 anchors/claim,16 pending jobs and one building job. Sidecar JSON<=2MiB.
IPC response<=1MiB, limit1..50/default20, query<=2048 UTF-16 units **and** strict
formatted token limit,200 candidates/lane,3 displayed evidence items/film,
1000 codepoints/excerpt. Resource exhaustion is visible rejection/partial
diagnostic, never silent successful truncation. These are pilot bounds, not
demonstrated capacity. Schema maxItems alone does not authorize larger work.

Requests/responses below omit envelope fields for readability. Required fields
are shown; `?` marks optional/default fields in descriptive notation.

| op | Request fields | Successful reply payload |
|---|---|---|
| film_capabilities | none | capability:"film_v1", schema_version:1, ready:bool, formats:["text","markdown","srt","vtt","pdf"], modes:["source","hybrid"], limits:{as above}, normalization_version, ranking_version, embedding_identity, film_revision, film_epoch |
| film_add_collection | path:absolute clean selected directory; exclusions?:[] relative prefixes | accepted:true, collection_id:opaque UUID, film_revision, film_epoch |
| film_set_exclusions | collection_id, exclusions:[] | accepted:true, film_revision, film_epoch; invalidation complete before ack |
| film_remove_collection | collection_id | accepted:true, removed:true, film_revision, film_epoch; dependents/staging/caches invalidated before ack |
| film_import_manifest | collection_id, relative_path, expected_sha256 | accepted:true, job_id, job_state:"pending", film_revision, film_epoch |
| film_import_analysis | collection_id, relative_path, expected_sha256 | same accepted-job payload |
| film_cancel_job | job_id | accepted:bool, job_state; accepted only pre-promotion/owned session |
| film_reconcile | collection_id? (all selected if omitted) | accepted:true, job_id, job_state:"pending", film_revision, film_epoch |
| film_remove_analysis | collection_id, film_id, edition_id, analysis_id | accepted:true, removed:true, film_revision, film_epoch |
| film_status | job_id? | status:{shape below} |
| film_search | text, mode:"source"/"hybrid", collection_id?:null, filters?:{language?:null,edition_id?:null,source_kind?:null}, limit?:20, spoiler_policy?:"hide"/"allow"/"exclude_spoilers" (default hide) | results:[], film_revision, film_epoch, ranking_version, embedding_identity, limited:bool, warnings:[] |
| film_evidence | reference:{shape below}, expected_film_revision, spoiler_policy?:"hide"/"allow" (default hide) | evidence:{shape below}, film_revision, film_epoch |
| film_navigation | reference, expected_film_revision, action:"open"/"reveal", spoiler_policy:"allow" | navigation:{token,snapshot_path,page,action,source_revision,film_revision,film_epoch,label:"Verified source copy"}; no process launched by service |
| film_release_navigation | token | released:bool; owned by issuing socket only |

```json
{"status":{"ready":true,"model_ready":true,"busy":false,"paused":false,"phase":"ready","error":"","film_revision":"7","film_epoch":"12","collections":[{"collection_id":"c","path":"<private-selected-root>","exclusions":[]}],"counts":{"editions":2,"sources":3,"units":9,"passages":3,"analyses":1,"claims":2,"vectors":5,"errors":0},"jobs":[{"job_id":"j","kind":"manifest","state":"complete","stage":"committed","done":3,"total":3,"error_code":"","error":"","warnings":[]}],"warnings":[],"coverage":{"watch_complete":true,"limited":false}}}
```

Jobs states: pending/running/paused/complete/failed/canceled/interrupted;
stage: admission/snapshot/parse/validate/chunk/embed/reauthorize/committed.
Status carries at most20 recent jobs/warnings plus counts for omitted failures;
job_id selects its detail. Building/warnings do not imply full-film coverage.
`embedding_identity` is the film identity object/digest including legacy producer,
weight hashes, helper/runtime digests, formatter, normalization, chunker,
facet renderer, max_tokens2048, dimension768, dtypefloat32 and host byte order.
Ranking version is separate: `semanticlens-film-rrf/1`.

```json
{"film_id":"film:harbor","edition_id":"edition:draft-1","collection_id":"c","display_title":"Harbor","year":null,"film_revision":"7","film_epoch":"12","source_exact":false,"identity_match":false,"suggestion":true,"lanes":[{"lane":"analysis_vector","rank":1,"score":0.6,"analysis_id":"analysis:1","claim_id":"claim:duty"}],"evidence":[{"reference":{"collection_id":"c","film_id":"film:harbor","edition_id":"edition:draft-1","source_id":"source:script","source_revision":"<sha256>","unit_id":"line-5","start_codepoint":0,"end_codepoint":49,"analysis_id":"analysis:1","claim_id":"claim:duty"},"source_excerpt":"<exact canonical substring>","analysis_text":"<separate imported interpretation>","origin":"authored_fixture","basis":"source_interpretation","semantic_review":"unreviewed","coverage":"full","source_kind":"screenplay","edition_relation":"exact_edition","page":null,"cue_start_ms":null,"cue_end_ms":null}],"warnings":[]}
```

The text-bearing result example above is an explicit `spoiler_policy:"allow"`
response. Default hide returns null source_excerpt/analysis_text/unit_text and
`spoiler_redacted:true`, including source-only hits. Source material has no trusted
spoiler classification; imported claims cannot authorize its disclosure by
labeling themselves spoiler-free. Preserve titles, references, locator/provenance
and lane metadata so the user can choose a reveal gesture without seeing content.
`film_evidence` defaults hide independently of the previous search request; the
native Reveal evidence gesture sends allow. Never rely on UI-only redaction.
The native mode notice states that ranking can remain spoiler-informed even
when excerpts are hidden; hiding text is not a claim of spoiler-free retrieval.

For pure source hits, analysis_id/claim_id are null; all evidence reference fields
through offsets are required. Quote/sample values above are structural examples,
not fixture identity/expected scores. `film_evidence` treats references as untrusted:
look up exact same collection/work/edition/revision/unit, require active eligibility,
verify expected film revision, then reauthorize every dependency. For analysis
references require that its stored anchor matches; client cannot fabricate an
arbitrary quote relation. Return bounded unit_text/source_excerpt, actual offsets,
relative_path, source_kind/language/edition_relation, page/cue locator, plus stored
analysis provenance/coverage/limitations/not_observable and interpretation where
applicable. No arbitrary path input is accepted for evidence.

Source lexical/vector and analysis vector are independent lanes. RRF uses best
eligible film hit per lane, one term per film/edition/lane, constant60, source
lexical/source vector/analysis vector weights1, curated metadata0.5. Curated lane
may be empty; analysis lexical matches are labeled analysis_text_match. Verified
literal source phrases get a precedence tier; title matches are identity_match.
Reject duplicate overlapping votes and use deterministic identity tie breaks.
`exclude_spoilers` excludes every analysis claim whose spoiler_level is not
`none`, and keeps source text redacted. `mode:"source"` forces source lanes.
Always show suggestions until an abstention cutoff has
actual frozen evaluation support; scores are not confidence percentages.

The service late reply gate extends to film_search/film_evidence: suppress if
canceled/disconnected or response film_epoch differs from current atomic epoch.
The UI maintains separate film request IDs/results/revision/epoch. On collection,
mode, filter, text, policy or revision change cancel outstanding requests, clear
evidence and ignore old replies. Poll film_status/capabilities separately from
legacy readiness; absent film capability disables film mode. Cache keys contain
film revision, epoch, source revision and full evidence reference.

Native S1 source navigation is a `film_evidence` lookup on click, showing exact
source text after explicit reveal, source asset, unit/page/cue locator and separate
interpretation as plain text. It does not reuse legacy media membership. A source
path returned for display is not authority to call legacy openResult/reveal.

`film_navigation` verifies active reference, expected revision, explicit allow,
all analysis dependencies and a fresh source lease. Copy the raw source into an
owned random0700 subdirectory under `<data>/film-open`, compare hash and current
source lease again, finish the copy with permissions0400, then return only that
service-issued snapshot path and an opaque per-socket token. The response must
pass the film epoch serialization gate. Backend accepts it only for its pending
navigation ID/reference/current revision; launches `okular --page N snapshot`
for PDFs or QDesktopServices::openUrl for the verified copy. Reveal uses fixed
`dolphin --select snapshot` argv. No shell, arbitrary executable or remote URL.
Labels say Open/Show verified source copy; do not claim that the copied file is
the live original. The raw source basename is sanitized; the snapshot's extension
comes only from the verified accepted source type.

Navigation copies remain owned by service until explicit release, issuing socket
disconnect, source/collection invalidation or shutdown. One live copy per socket,
max16 total and32MiB each; replacement releases the old copy. Backend releases on
navigation/pane replacement, disconnect and teardown. Service startup removes
interrupted owned snapshot directories safely. Existing files are never adopted
as capabilities. Cleanup and watcher invalidation happen before removal ack.
External desktop programs deliberately opened by the user have independent
lifetimes; deletion cannot revoke bytes they already read. S1 tests use a fixed
capture opener/plan instead of launching real apps, and separately validate the
native explicit action on owned fixtures. No guarantee of permanent original-path
authorization or revocable external-app copies is made.

## Implementation consumers, branches and stop rule

Parent core/service owner: create FilmStore on the existing worker, add safe
callbacks and separate epoch/status, implement complete operations and stage/step
lifecycle. Adapter owner: pure format normalization/mapping only, keep supplied
IDs separate. Parent UI consumer: separate film list/evidence state and asynchronous
operation IDs; import explicitly selected collection files with expected digest.
No implicit film KRunner extension is required; preserve runner-v5 unchanged.

Minimum falsifiers before consequential acceptance: wrong/remake/edition/revision
anchors; quote offset including emoji/combining text; contradictory quote text and
unsupported coverages; special/prefix token overflow with no producer request;
literal phrase only in analysis vs real source; literal crossing windows; bad
symlink/traversal/exclusion/provider; source replaced/edited at snapshot,
embedding, precommit and disclosure; cancellation queued/running/after commit,
disconnect, pause, shutdown and restart; migration failure leaves old index
identity/counts/search intact; malformed schema/duplicate IDs/supersession;
corrupt vectors; same revision idempotent reimport; late UI replies hidden.
Use source-faithful positives at each applicable phase before interpreting a
negative. Freeze corpus/source/candidate/revision and expected labels independently
of checker output. Reuse unchanged legacy receipts, then perform a focused combined
legacy regression on the actual candidate and disposable data/socket.

Outcomes: if strict controls/import/ranking/lifecycle/native evidence pass plus
uninvolved review, deliver bounded S1; if helper parity fails, stop film inference
and diagnose model/runtime/formatting; if migration fails, keep legacy usable and
report film-only failure; if source authorization changes, withdraw dependencies
and require explicit corrected reimport; if quality is weak, retain source-only
counterfactual and report bounded result rather than tune held-out expectations.
Same author repairs new in-grant versions; same uninvolved reviewer rechecks
affected claims. This report cannot approve its own semantics. Contract or
acceptance changes return to parent before final integration.

Stop at the reviewed offline authored-fixture S1 result. No provider, personal
corpus, spending, bulk capacity claim, new release or installed-rc2 mutation.
No child delegation. Compile lasted7.71s; owned controls elapsed5.73s (not a
benchmark, excludes source investigation/compilation). All owned processes and
tool handles are terminal; package model/token/cost usage is unavailable. Installed
user rc2 processes were left untouched. Total wall time recorded separately in
the final owned receipt only if measurable; investigation wall time is otherwise
unknown. No additional usage ledger was created.
