# Authored offline film pilot

Select this directory as the collection root. Import each subdirectory's
`manifest.json`, then its optional `analysis.json`. Manifest source paths are
relative to this collection root, not to the manifest's subdirectory.

All stories, analyses and query labels are newly authored fictional test inputs.
They establish neither real-film retrieval quality nor the benefit of an LLM.
The four analyses use `authored_fixture` provenance; no model generated them.
The six editions include two incompatible cuts of the same titled work, timed
SRT/WebVTT, untimed screenplay text/Markdown and a two-page extractable PDF.
The declared queries include literal source, analysis-only, cross-unit, PDF-page,
conceptual and no-answer cases. Conceptual outcomes are observations to report,
not a preexisting claim that the ranker succeeds. A no-answer query may return
suggestions; record its false positives instead of relabeling them as relevant.

Canonical units use `semanticlens-film-units/1`, retaining Unicode code points
without folding. Source and analysis digests and anchor text must be checked by
the actual importer; structural JSON Schema validation alone is insufficient.
Default unknown/major spoiler interpretations require a deliberate reveal.

Tests must copy these inputs into an owned disposable collection before editing,
renaming, deleting, excluding or injecting malformed sidecars. Preserve this
fixture version after it is frozen. `../generate_pdf.py` recreates the authored
PDF with ReportLab; the PDF embeds no third-party font file.

See `LICENSE.txt` for the CC0 dedication.
