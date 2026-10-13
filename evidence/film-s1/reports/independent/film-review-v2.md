# Independent affected adapter v3 recheck

Outcome: no blocking defect found in the affected overflow, rounding, header and timing-tail repairs at the standalone parser scope. This report supports parent consideration of those repairs; it does not accept or qualify combined S1. The reviewer remains uninvolved in production authorship and made no candidate/canonical edits.

Actual trigger: parent resumed the same S1 review on an automatic S1 continuation. Earlier partial review is delivered and immutable; combined core/UI review remains unfinished and must use the later frozen combined candidate. This recheck was limited to v3 parsing. No model/service/latency work was authorized or performed; core author retains that resource and measurement scope.

## Exact identity and contract delta

Reviewed frozen `local/film-adapters-v3/source/adapters.cpp` and corresponding active source SHA256 `b7ed891f4f9cd3b37a03fa58983f99e162932357d0388709967a0697b60704ff`; header SHA256 `3ce9cf71820cf07063c4577326eb0c04ad4aac8c9b8e9b963b94f9577063bac1`. Author report SHA256 `0c60895bb2d654b3be9f7d4fe0b0eb4ac7f9308c47b21018c50eb969cd83b39f`. Manifest SHA256 `a9baf93ea23a7e5cb3decc86504ac715095caf56bf204f55c9ae9a065f1b0f15`: all33 listed entries verify (34 artifacts when including the manifest itself).

Rechecked v1 review manifest `cfb862c9b85ffad1b49bcd2270406ba93ee89fbf4ca750d5e68b5495fa894b5f`, all19 entries unchanged. The retained predecessor v2 source equals the original frozen source. Source HEAD remains `9edcbc1cbfbbf55533fcc6117da896ead4e98bcd`, branch `codex/film-s1`; this is an uncommitted candidate whose identity is its pinned source, not HEAD alone. Current GOAL records the parent-adopted maximum exact millisecond bound.

Relevant delta: timestamp now returns qint64 directly, checks hour conversion, and bounds each cue endpoint before multiplying by3600000. Remainder is bounded by parsed minute/second/millisecond components; hours are nonnegative under the grammar. Therefore the checked maximum `9007199254740991` ms cannot overflow qint64 and is exactly representable in the existing JSON-number fields. Above-bound input visibly fails. No field type or schema change was made. Header signature now requires exact WEBVTT or ASCII space/tab metadata and forbids a timing delimiter; SRT timing tails reject. API, IDs, map fields, source_format ownership and normalization string remain unchanged. This is a narrowed input bound, not a playback-duration/capacity claim.

## Independent raw evidence

Compiled the retained independent JSON driver against the frozen v3 implementation with UndefinedBehaviorSanitizer. `inputs.jsonl` and `outputs.jsonl` contain all21 input/reply pairs; `checks.json` locates them and does not replace their raw fields. Nine source-faithful positives and12 rejection controls have no contradiction. The process exited0 and stderr is empty.

The five original counterexamples now return `ok:false`, no units and displayable errors: hour-conversion overflow; signed-multiplication overflow; unsafe JSON integer; WEBVTTjunk; SRT JUNK timing tail. This directly contradicts v2's fabricated successful cue fields. The original input bytes are reused from the frozen v1 reviewer record; original v1 evidence remains unchanged.

At `2501999792:59:00.991`, raw serialized output contains exact `cue_start_ms:9007199254740991` and `cue_end_ms:9007199254740991`. Adjacent valid start `.990` returns9007199254740990 with end9007199254740991. `.992` rejects both when used as start and independently as end. Largest qint64 hour text also rejects before multiplication. Expectations use exact Python integers and declared timestamp arithmetic; they were not derived from the candidate's conversion routine.

Five prior positives cover text/Markdown, SRT overlap with CRLF, VTT space metadata/NOTE exclusion/visible markup/entity, and extracted-PDF blank-page locators. Their complete unit JSON objects equal the retained v2 outputs. Independently rechecked raw_text byte ranges and per-scalar raw spans for emoji, combining text, Hebrew, CRLF and amp entity, applicable page/cue fields, source_format without source_kind and UTF16 span conversion. New upper-bound cue bodies include emoji/combining text and pass the same scalar-map checks. Additional affected controls accept tab header metadata and highest SRT format hour; reject non-ASCII header separator, header arrow, tab tail and an embedded timing tail. Unchanged byte/unit-count/UTF8 inventories are reused rather than rerun.

## Limits, recheck condition and closure

This demonstrates source review, standalone QtCore6.12.0 fixture parsing and exact Qt JSON serialization. It does not demonstrate database storage/readback, application import, source authorization, reference lookup, playback alignment, native UI, model behavior, film latency or quality. Tokenizer-seam conclusions and missing full-load raw arrays from review v1 are unchanged; no tokenizer/model controls were rerun.

Parent owns acceptance and integration. Return any further affected source/contract changes as a linked frozen successor to this same reviewer. Combined S1 privacy/identity/ranking/lifetime/migration/UI review remains pending the parent's exact frozen candidate. No production, canonical state or old immutable artifact was changed.

All owned compiler/control processes exited and tool handles are terminal. No socket, service, model, database, watcher, provider, personal corpus, installed rc2 process or external application was opened. No descendants. Compile observed4.725482156s; driver/readback/assertion script0.010312947s, neither is product/film latency. Total investigation elapsed, model tokens and cost are unknown. No usage ledger created.
