# Frozen retrieval corpus v1

Created 2026-10-11 before any retrieval against these inputs. This is a small,
authored synthetic acceptance corpus, not a representative population of users'
files or evidence of photographic search quality. All corpus text, PDFs, vector
artwork, rendered images and query labels are original project demo material,
dedicated under CC0-1.0; see `LICENSE`. No personal data or borrowed photographs
are present. Eight source SVGs are included; their actual PNG bytes are frozen.

There are 28 neutrally named files: 16 text documents (eight intended targets and
eight confusable alternatives), four two-page text PDFs, and eight untagged PNG
screenshots/charts. The PNGs contain no descriptive filenames. They deliberately
include opposites: rising/falling bars, failed/successful payment, off/on backup
and timeline/pie chart. The screenshot identifier also checks the OCR path.

`queries.json` predeclares 37 queries: 20 semantic queries (eight document, four
PDF, eight image), six deliberately unique identifiers, eight literal controls,
and three queries for which the corpus has no relevant file. Labels were written
from the source content and artwork before inference, not from search output.

The primary scorer is file hit@5: a query passes when any predeclared relevant
file appears among the first five distinct files. Each query here has one relevant
file. Use default unfiltered service search with limit 5; use native Baloo search
with the identical query as typed, content indexing enabled and limit 5. Do not
rewrite queries for either engine or use modality labels as a search filter.
Report each semantic modality separately, aggregate semantic hit@5, literal and
exact hit@1/hit@5, and the semantic percentage-point difference versus Baloo.
For KDE Index PDF results also report whether the returned page matches the label;
Baloo does not provide a matching-page result, so its file hits remain comparable.

The existing release target stays: >=80% semantic file hit@5, >=15 percentage
points more than Baloo on that subset, and every unique identifier retrieves its
intended file within five results. Report hit@1 as well. None is achieved yet.
No-relevant queries are excluded from recall denominators; report the number and
scores of their returned false positives. Nearest-neighbor results do not prove
the existence of a relevant file, and no relevance cutoff has been validated.

Freeze the service/build identities before evaluating. Record the exact Baloo
version, content-index completion and extraction state for every supported file.
An unsupported PNG/OCR modality is a measured capability difference, not a
configuration failure; text/PDF content indexing failures must be disclosed.
Record warm and cold end-to-end query timings, indexing time, RSS, index size and
worker closure. Repeated latency queries do not count as additional retrieval
samples. Corpus, labels and scoring criteria must not change after seeing results;
any necessary correction requires a linked, explained successor benchmark.

`SHA256SUMS` pins all inputs, labels and this contract. Evaluation results belong
under a fresh `evidence/retrieval-*` directory, outside these frozen inputs.
