# Offline film-search pilot (S1)

This development feature adds a separate film collection to SemanticLens.
Scripts and subtitles are indexed locally. Optional analysis JSON files add a
distinct interpretation lane; the application does not generate analyses or
contact a provider. Both its legacy producer and the optional shared-model query
worker run EmbeddingGemma locally.

S1 is an offline development pilot validated on authored, redistributable
fixtures. It is not a claim of measured real-film retrieval quality or a
bulk-library rollout. The published rc2 release does not contain this feature.
See the [validation scopes and measurements](FILM_VALIDATION.md). The pilot is
delivered for review; it has not been merged, released or installed over rc2.

## Try the development build

Build with the normal CMake process. The default build includes the service,
native app, `semanticlens-film-tokenizer` companion and experimental
`semanticlens-film-worker`. The tokenizer counts the
exact formatted model input before embedding; film input must fit the qualified
2,048-token context. The shared worker uses one text model with separate query
and indexing contexts; the tokenizer loads vocabulary only. The private validated
native snapshot has qualified two- and six-thread worker configurations. Its
frozen two-thread pilot passed the one-second warm-query p95 target while
indexing. These measurements apply to that exact snapshot and corpus. The snapshot remains
local for owner review because its helpers contain private build-path strings;
it is not uploaded with the public source/evidence export.

A new source build has different executable hashes and does not inherit that
binary qualification. The service reports unfamiliar producers/workers as
unqualified, even when their own model labels match. That preserves honest
provenance. Its fresh embedding producer passed the eight reference-vector
controls; the rebuilt shared worker has no new concurrency or timing claim.

For an isolated pilot, select an existing verified model directory and embedding
producer explicitly. Keeping the installed rc2 producer avoids changing its
binary identity; do not replace that binary or reuse a live index as a test.

```bash
# Set these to the verified model directory and producer you intend to use.
MODEL_DIR=/absolute/path/to/models
LEGACY_PROBE=/absolute/path/to/embedding-probe
mkdir -p local/film-demo
build/bin/kde-index --film-view \
  --models "$MODEL_DIR" --probe "$LEGACY_PROBE" \
  --data-dir "$PWD/local/film-demo/data" \
  --socket "$PWD/local/film-demo/service.sock"
```

Use an absolute socket path and a dedicated data directory. The separate
`film.sqlite` contains derived film data; ordinary file search retains its
existing database and producer identity. The installed user application and
Baloo are not test resources.

Choose **Films · pilot**, then **Collections → Add collection**. Select the
`film/fixtures/v1` directory or a private copy of it. Select that collection in
the dropdown, then use **Collections → Import sources** to import one of its
`manifest.json` files. Import its `analysis.json` separately if supplied. An
accepted import is a queued job; check its final state before searching.

The supplied fixtures contain short synthetic scenes and test analyses, rather
than real-film scripts or a measured LLM-analysis batch. Read their
[fixture description](../film/fixtures/v1/README.md).
Use the supplied fixtures when reproducing the pilot validation.

## Search and inspect a match

**Source only** searches supplied source passages. **Hybrid** adds imported
interpretations and eligible curated metadata. Results are grouped by explicit
film and edition IDs: identical titles do not merge distinct cuts or drafts.
Literal source matches take precedence and are distinguished from interpretation
text matches and ranked suggestions.

Unverified prior interpretations are excluded from normal hybrid search. To
explore them, explicitly check **Explore unverified prior interpretations**.
This enables a source-less interpretation lane; it does not establish source
grounding. Switching to **Source only**, leaving Films or reconnecting clears
that opt-in. Changing it clears the current results and revealed evidence before
searching again. Reveal alone never enables prior participation.

At compact window sizes, the same option is labeled **Explore unverified priors**.

Select a result, then explicitly reveal an evidence item. Source text and analysis
text are hidden by default, including during ranking. Ranking may nevertheless
use spoiler-bearing content. Hidden text is not a promise of spoiler-free ranks.

The evidence panel identifies the supplied source, edition relationship, unit or
PDF page, analysis origin and declared coverage. Imported interpretations remain
unreviewed interpretations, even when their quotation anchors validate. An
`unverified_prior` has no source anchor and cannot open a source. Subtitle offsets
describe the supplied file; playback alignment has not been established.

**Open verified source copy** and **Show verified copy in Dolphin** request a
fresh, hash-verified private snapshot. They do not open an unchecked analysis URL
or shell command. Copies are temporary and belong to the requesting session;
they may disappear when selection changes or the app disconnects. They are not
editable copies of the original file.

Opening a PDF at its matched page requires Okular. If it is unavailable, the app
reports that limitation and releases the unused copy. The evidence panel still
identifies the page and passage, and Dolphin can reveal a verified copy for use
with another viewer. An external application may retain content it has already
opened after the temporary file is released.

## Import requirements and recovery

See the [manifest schema](../film/schemas/film_manifest.schema.json),
[analysis schema](../film/schemas/analysis.schema.json) and
[current development contract](FILM_S1_CONTRACT_V7.md). Versions
[1](FILM_S1_CONTRACT.md) and [2](FILM_S1_CONTRACT_V2.md) preserve the earlier
contract history; [version 3](FILM_S1_CONTRACT_V3.md) records prior-search and copy
repairs; [version 4](FILM_S1_CONTRACT_V4.md) records strict evidence references.
[Version 5](FILM_S1_CONTRACT_V5.md) introduces the query execution separation;
[Version 6](FILM_S1_CONTRACT_V6.md) records its cancellation, scan scheduling and
legacy-producer provenance repairs. Version 7 extends honest provenance to
unqualified selected workers.
The native pilot requires the version-2 ranking policy and its
explicit default-exclusion capability; it does not silently connect to an older
film service with different prior semantics. Source paths are relative to the
explicitly selected collection. Every source revision is the SHA-256 of its
exact bytes. Interpretations must identify the correct film, edition, input
sources, normalization and quoted codepoint ranges.

The supported source formats are UTF-8 text, Markdown, SRT, the supported WebVTT
subset and PDFs with extractable text. Markdown is literal source text. WebVTT
cue settings, styles and unsupported markup are rejected visibly. Raw mappings
preserve Unicode scalars and do not silently normalize combining text. PDF unit
byte mappings refer to extracted text, while source revision hashes refer to the
original PDF.

An import stages its content and promotes it atomically. Failed or canceled jobs
must not replace the active edition. After an interrupted job, inspect its status
and explicitly reimport the unchanged sidecar. Editing, renaming, deleting or
excluding a source can withdraw its results and dependent analyses; cached text
is not authority to reveal an obsolete source. Use **Refresh** to request
reconciliation. Removing a collection deletes its active searchable film data
without modifying originals.

The pilot has bounded source, unit, vector and job counts. Reported limits and
errors are part of the feature; they are not a 10,000-film capacity claim. Ranked
suggestions can include irrelevant films, including for no-answer queries. There
are no confidence percentages or calibrated absence guarantees.

Cloud enrichment, real-film quality trials and bulk capacity qualification require
separate review and authorization after S1. No account, provider credential,
telemetry or remote generation is required for this offline path.

The [proposed S2 quality comparison](FILM_QUALITY_PLAN.md) records the design's
follow-on evaluation and its decision boundary. It has not been run.
