# Offline S1 validation

The pilot adds explicit film collections, UTF-8 screenplay/Markdown, SRT,
supported WebVTT and extractable PDF sources, plus separately imported analysis
JSON. Search, indexing and embeddings run locally. The validation corpus contains
six fictional editions and four synthetic analyses; no provider calls, real-film
quality experiment or bulk rollout were performed.

The private native development snapshot and a fresh source build are distinct artifacts.
The snapshot contains the exact binaries used for the reviewed integration and
timing evidence. A fresh source build compiles and installs correctly, but new
binary hashes do not inherit the snapshot's worker qualification. The service
labels unfamiliar workers and producers unqualified. Neither artifact changes
the installed rc2 application or Baloo. The validated native snapshot remains
local for owner review: its tokenizer/worker embed private build paths, so their
exact binaries are excluded from the public evidence tree. Public delivery uses
source, sanitized records and hashes; those hashes do not substitute for binaries.

## Evidence scopes

| Requirement | Reviewed evidence and practical limit |
| --- | --- |
| Source identity and references | Explicit work/edition/source IDs, source hashes, Unicode codepoint anchors, paired analysis/claim IDs and nonempty spans. Malformed and stale references reject without returning evidence. |
| Ingestion and interpretation | SRT/VTT/text/Markdown/PDF controls, strict schema and token limits, duplicate/superseding imports, separate source/analysis lanes and literal-source precedence. Interpretations remain contestable; prior interpretations require explicit opt-in. |
| Ranking and negative controls | Raw source/analysis replies distinguish source literals from interpretation matches. Repeated anchored claims do not add lane votes; misleading interpretations remain unreviewed. Negation and no-answer queries can return suggestions with an explicit absence-calibration warning. These are controls, not retrieval-quality success. |
| Privacy and lifecycle | Selected-root/descriptor checks, traversal and source-swap controls, atomic promotion, interruption/recovery, isolated ENOSPC, derived-vector corruption, wipe/reimport and film-database failure isolation. These are bounded controls, not all possible races or forensic erasure. |
| Worker correctness | 146 exact vector-byte comparisons across two and six threads; separate shared-model/context, executing-cancellation/reuse and publication-race controls. Transport pending states are not substituted for actual model execution. |
| Full-source coverage | All 512 passage vectors match their own stored input rows; all 2,048 physical source lines are covered, including the final unit. Text/image indexing progresses before film completion. Instrumented duration is not quiet throughput. |
| File-search compatibility | Rc2-created index reopened with S1; IDs, identity types and all six full vector blobs stayed identical through film imports and concurrent clients. Text, PDF page, OCR, image and private KRunner backend checks passed. This is not a new graphical KRunner Enter test. |
| Native interface | 79 controls on isolated KWin cover reveal, prior policy, compact layout, concurrent clients, stale-byte disposal and reconnect. A separate actual schema-job error appears in the opened PlainText status dialog. Empty warning lists do not establish warning-dialog rendering. |
| Actual source opening | Standalone app opens verified text/PDF copies in real Konsole/Okular consumers. Fresh action/reference and copied bytes, matched PDF page, native painting and cleanup are recorded. Dedicated virtual KDE evidence does not establish physical-display scanout or arbitrary handlers. |
| Automatic startup | Real app starts adjacent service/tokenizer/worker, with the parser's default six-thread budget corroborated by actual worker argv and query provenance. Its empty-profile supervisor query proves startup, not retrieval quality. |
| Clean build and installation | Fresh checkout/runtime/build tree compiles. A failed 22-file stage exposed missing install rules; the repair installs all 24 runtime/notice files without the upstream SDK. The new producer passes eight original reference controls, malformed-input and recovery checks. |

## Fixed timing experiment

The criterion and inputs were unchanged after the initial failure: eight frozen
queries × source/hybrid × three repetitions, 48 samples per phase. A query is
eligible for the indexing phase only when the same job reports running/embed
before and after its request-to-complete-reply interval. Phase checks and logs
stay outside that interval; percentile uses nearest rank.

| Phase | Result | Scope |
| --- | ---: | --- |
| Original indexing run | 2,116.779222 ms p95 | Failed the unchanged 1,000 ms target; retained. |
| Repaired warm idle | 165.972001 ms p95 | Exact qualified native snapshot, CPU budget 2. |
| Repaired warm indexing | 169.855515 ms p95 | Passed the same target and 48-query matrix. |
| After observed model/process unload | 2,144.450671 ms | Separate single cold-reload query; not disk-cold. |

These measurements do not promise latency for every mixed image/file workload,
another compiler or binary, another CPU budget, real films or a large library.
The execution checks and timed workload are separate evidence categories.
Memory and index-size observations are phase snapshots, not peak capacity claims.

## Reproduction and review boundary

Build using [BUILD.md](BUILD.md), then use an absolute socket path and a dedicated
data directory as described in [FILM_PILOT.md](FILM_PILOT.md). Import only the
redistributable [pilot fixtures](../film/fixtures/v1/README.md) for reproduction.
Preserve the existing producer when opening an existing file index; binary or
representation changes require an explicit compatible profile or rebuild.

The [public evidence](../evidence/film-s1/README.md) contains a disclosed subset
with original and derivative hashes, independent reports, raw API replies and
isolated native captures. Verify its delivered files with
`(cd evidence/film-s1 && sha256sum --check SHA256SUMS)`.
Sanitized paths are not original execution paths. Full internal immutable
records retain failure attempts, source pins, raw results and reviewed scope
corrections; excluded model weights, indexes and large observer dumps are not
represented as publicly replayable artifacts.

Automated UI/app-launch tests use isolated KWin, D-Bus, display and profiles.
No test may silently fall back to a personal desktop. Hosted CI checks a clean
build, staged installation and model-free offscreen startup; it does not replace
model-backed target evidence.

The proposed [S2 comparison](FILM_QUALITY_PLAN.md) requires separate authorization.
S1 does not establish a benefit from LLM
analysis, calibrated no-answer behavior, playback alignment, cloud enrichment,
10,000-film capacity or a new release. Stop at this review boundary.
