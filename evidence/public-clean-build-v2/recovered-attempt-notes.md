# Validation harness note

The first read-only mode/hash inventory pointed at the source parent instead of the extracted `kde-index-source` directory and therefore recorded all expected paths as missing. The check did not alter source or stage. The corrected path produced a complete 1,837-file readback with zero missing paths, hash mismatches or mode mismatches; see `extracted-source-check.json`.
