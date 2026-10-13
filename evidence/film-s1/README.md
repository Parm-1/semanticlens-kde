# Offline film-search pilot evidence

This is a selected, sanitized S1 record for review, alongside the [source guide](../../docs/FILM_PILOT.md) and [validation scopes](../../docs/FILM_VALIDATION.md). It establishes authored-fixture functionality and bounded target behavior. It does not establish real-film quality, an LLM benefit, cloud enrichment, bulk capacity or a new release.

## Read the evidence

- [Independent scope reports](reports/independent/) and [readbacks](review/).
- [Final delivery review](reports/independent/delivery-v19.md) and [coverage readback](review/delivery-v19-coverage-readback.json) identify the accepted scopes and repaired export/receipt-label findings.
- [Fixed timing](timing/): original failure, unchanged criterion, corrected 48 idle and 48 indexing request/reply intervals. Two-thread warm indexing p95 is 169.855515 ms; cold process/model reload is a separate single 2144.450671 ms query.
- [Rc2 compatibility](legacy/): saved IDs, identity types and full vector bytes before and after film imports.
- [Native controls](native79/), [actual text/PDF opening](opening/), [automatic worker startup](auto-worker/) and [nonempty status error](f19/). All graphical runs used dedicated isolated KWin sessions and authored fixtures.
- [Preflight scope correction](opening/PREFLIGHT_SCOPE.md) distinguishes the repaired-positive GIO receipts from the separately retained original failed attempt.
- [Build and service reproduction](build/): clean build, failed then repaired 24-file install, new-producer reference results and 69 serial API controls.
- [Full API replies](build/service-reproduction-full-api-records.json) and their [receipt](service-records-receipt.json) supplement the selected export with all 300 request/reply pairs from the corrected serial run, including misleading-analysis, negation, repeated-claim and no-answer controls.
- [Source binding](source-binding.json): 23 selected frozen production bodies match public commits 207dac and 4b9bb. This is source equivalence, not proof of the historical native compilation origin or all toolchain inputs.

## Identity, omissions and verification

[hash-receipt.json](hash-receipt.json) maps **155 selected files** (146 textual/structured records and nine screenshots) to original and derivative SHA-256 values. Text sanitization only replaces checkout/home prefixes with `/workspace/semanticlens` and `/home/testuser`. Other numeric, vector, quote, cue, query and control bytes are retained. The additional full-API supplement and root metadata files sit outside the 155-file subset and are included in SHA256SUMS.

Original manifests can refer to omitted files. Model weights, databases/WAL, caches, executables and large observer dumps are omitted. This is **not a complete replay**. Embedded binary paths were not rewritten: the exact qualified 24-file native snapshot remains local-private. References to that unavailable archive in the export receipt identify it, not a public download. Fresh source builds have different hashes and do not inherit its worker or timing qualification.

The receipt's `generated_artifacts` section identifies metadata in the original private export, including its original README and assembly script. It does not describe this new public README or promise that those private-export metadata files are present here. Use its `items` entries for the 155 payload mappings and SHA256SUMS for this complete delivered directory.

[DELIVERY_REPORT.md](DELIVERY_REPORT.md) and [lifecycle-controls.json](lifecycle-controls.json) record the local export method and its existing-output/symlink rejection controls. That method depends on private original inputs; it is not a public reconstruction recipe. Public readers can verify every delivered file from the repository root:

```sh
(cd evidence/film-s1 && sha256sum --check SHA256SUMS)
```

To reproduce product behavior, follow the source build and authored-fixture commands in the pilot guide. Hosted CI covers build, staged installation and model-free startup; it does not substitute for model-backed native evidence. Preserved failure attempts and scope corrections are separate observations, not a combined passing count. Stop at S1 review before S2/S3.
