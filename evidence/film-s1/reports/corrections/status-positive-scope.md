# Erratum: v4 elapsed-time values

This linked correction applies only to the elapsed-time sentence in `local/film-status-positive-v4/REPORT.md`, SHA256 `cf44f89a6f529b6acd22bd07d783a7f789086df9cf8924ae636d6d98b197aa74`. The original v4 33-entry manifest SHA256 is `ce224b5d5f138eb4839cff40be4bf990267bf75e6b692cf6ee8b98a1f1f8c6f1`; its byte-identical copy here is `source-v4-SHA256SUMS`.

The v4 report accidentally repeated predecessor v3 timings. The correct v4 values, read from the pinned original receipts, are:

- Outer dedicated-run elapsed: **9.652944343 seconds**, from `film-status-positive-v4/run-v1/evidence/dedicated-run-summary.json`.
- Inner runner elapsed: **8.122099315 seconds**, from `film-status-positive-v4/run-v1/evidence/run/run-summary.json`.

These measure setup, execution and closure for this focused run; they are not latency samples. Model/tool usage remains unavailable. All other v4 report text and evidence references remain unchanged. The copied original manifest binds all 33 v4 source, binary, raw receipt and capture pins; `REFERENCES.sha256` directly binds the report, manifest and decisive original raw files.
