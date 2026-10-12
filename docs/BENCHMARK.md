# Retrieval evidence

The corrected public candidate repeated the unchanged 28-file/37-query corpus:
20/20 semantic top-five hits (17 first), all six identifiers and eight literal
controls first, and all four labeled PDF pages returned. Its warm service-query
p95 was 47.236 ms over 185 exchanges. The comparison reuses the original Baloo
raw baseline, with 0/20 semantic hits, rather than a newly collected paired run.
All three no-relevant queries still return 15 irrelevant suggestions; concrete
rank-two confusions remain. See [current raw results](../evidence/retrieval-v2/report.md)
and the [independent recomputation and limits](../evidence/retrieval-review-v2/report.md).
The reported sampled RSS peak is 1,183,568 KiB and transient pre-shutdown apparent
index size 1,412,976 bytes; neither has a retained sample/file endpoint trace.
The separately inspected post-shutdown database is 237,568 bytes. These are
different observation scopes, not whole-app memory or scaling estimates.

## Original paired comparison

The first comparison uses 28 authored CC0 files and 37 frozen queries in
[`benchmark/`](../benchmark/). Queries, labels and targets were frozen before
retrieval. The files include text, four two-page PDFs and eight charts/screenshots;
they do not represent a sample of real personal folders or photographic images.

Both engines received identical typed queries with a five-file limit. Baloo
content indexing completed for all 28 files in an isolated index; ordinary native
AND query semantics were preserved. The service used its default hybrid ranking.

| Query subset | KDE Index first / top five | Baloo first / top five |
|---|---|---|
| Semantic text | 6/8 / 8/8 | 0/8 / 0/8 |
| Semantic PDF | 4/4 / 4/4 | 0/4 / 0/4 |
| Semantic image | 7/8 / 8/8 | 0/8 / 0/8 |
| All semantic | 17/20 / 20/20 | 0/20 / 0/20 |
| Unique identifier | 6/6 / 6/6 | 5/6 / 5/6 |
| Literal control | 8/8 / 8/8 | 8/8 / 8/8 |

All four semantic PDF matches identified the labeled source page. The missing
Baloo identifier appears in an OCR screenshot. This supports added semantic,
visual and OCR retrieval on these inputs, alongside exact-text search. It does
not establish superiority for filename search, every Baloo query syntax, large
collections, photographs or typical user queries.

All three no-relevant queries returned five irrelevant KDE Index suggestions each;
Baloo returned none. One disabled-backup query ranked the enabled-backup screenshot
first and its relevant file second. There is no validated abstention cutoff or
confidence score. Check the source before relying on a suggestion. These frozen
negatives must not be reused to fit a cutoff and then claimed as independent
validation.

The Ryzen 5 3600 CPU service had a warm-query p95 of 42.055 ms across 185 repeated
queries (five passes of the same 37 cases). This measures local IPC send through
decoded response, excluding the GUI. Baloo's CLI p95 was 22.724 ms, including
process startup; these are different timing scopes. Three queries after confirmed
model-worker unload took 1.957–2.750 seconds, including model reload, without an OS
cache flush. No GPU measurement is claimed.

Indexing took 21.845 seconds for 28 files/38 vectors after initial model readiness;
Baloo took 6.367 seconds including daemon startup. Reported sampled summed RSS
peaks were 1,127,164 KiB for the service tree and 69,848 KiB for Baloo. This is
50 ms sampling, may double-count shared pages and miss short peaks, excludes GUI
and observer memory, and has no retained raw sampler trace. Reported transient
pre-shutdown apparent index sizes were 1,412,976 and 139,264 bytes; they include
active database files and are not allocated disk sizes or large-folder estimates.

The [frozen raw result](../evidence/retrieval-v1/report.md) and
[independent review](../evidence/retrieval-review-v1/report.md) retain exact
identities, recomputation, source/page checks, clocks and limitations. The review
confirmed the retrieval totals and warm-latency arithmetic at this narrow scope.
The [CPU proof](../evidence/embedding-v1/report.md),
[service review](../evidence/engine-review-v2-portable/DELIVERY.md) and
[clean installed successor](../evidence/clean-build-v3/report.md) cover separate
correctness and installation boundaries. None establishes public release completion.
