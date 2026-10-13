# S1 film contract v3 — reviewed prior/copy repairs

Linked to frozen v2 (58940268dd7abb42fefc695b72b305f752a864dab18a2cdd417e9f30f778571d).
Parent commissioned the same author after independent partial review v3
(54d9b0cc985a4b4a5431643f09788413142133dcd45236d943294d7ab8400014).
This repairs two findings, preserves issue2 default prior exclusion, and makes
watch status describe the current serialization phase. Other v2 operations and
source/analysis-only reference shapes remain unchanged. No S1 acceptance.

## Exact additive search contract

film_search accepts optional include_unverified_prior: JSON boolean, default
false. In hybrid mode false/absent skips every unverified_prior claim before both
analysis_lexical and analysis_vector eligibility. Grounded source_interpretation
continues to contribute. true explicitly enables the exploratory prior view.
The analysis_only reveal extension never implicitly enabled search participation.
Source mode continues to exclude all analysis; true with source rejects
invalid_request as contradictory intent. false/absent with source is valid.
Null, string, number, object, array and undeclared aliases reject.

Successful film_search adds include_unverified_prior: effective boolean.
ranking_version is semanticlens-film-rrf/2 in capabilities, rows and responses.
The capability addition is:

```json
{"search_options":{"include_unverified_prior":
 {"type":"boolean","default":false,"modes":["hybrid"]}}}
```

No embedding/DB identity change or reimport is required: existing prior vectors
remain stored but are ineligible by default. Native UI must make opt-in an
explicit exploratory choice, clear it in source mode, and verify the returned
effective flag. Explicit Reveal continues to obey spoiler policy and source
reauthorization; it does not toggle this search option. Prior-only navigation
still rejects and no source quote is invented.

## Snapshot completion boundary

Engine's FilmLease callback still holds the authorized source descriptor and
records its raw SHA. It now requires successful QFile::flush with NoError,
checks error after close, reopens the owned destination, reads it completely
within the source size/cancellation bounds, and requires exact copied size/SHA.
Directory permissions are also checked. Extraction/navigation receive a lease
only after that completion/readback. Original inode/hash reauthorization remains
an independent condition; it cannot substitute for destination verification.
Failure destroys the unissued temporary copy and returns no navigation token.
This verifies completed copied bytes, not power-loss durability/fsync semantics.

The actual late buffered-write fault witness includes QFSFileEngine::write and
QFileDevice::flush in its stack. It is a syscall injection, distinct from the
separate real bounded tmpfs-full experiment. Normal tiny-copy bytes/hash, failed
flush/no token/no text/cleanup, legacy identity/vector preservation and subsequent
normal-path recovery are retained in local/film-store-v2/affected-v1.

## Current watcher coverage

At every film_status serialization (including job-specific replies), the service
compares current Engine requested paths against QFileSystemWatcher's installed
file/directory lists. watch_complete is false for a newly selected path before
registration, including before the next500ms synchronization. limited also becomes
true and watch_error reports registration pending or the observed capacity/path
failure. Once all current paths are installed, it is true and the stale error
clears. This is a Qt watcher-observer phase claim, not proof that a pathname's
bytes are immutable. Every evidence/navigation operation still requires its
own held-source authorization/revision and captured-epoch deferred reply gate.

## Consumer and stop boundary

The affected candidate source/bin/report is separately frozen under
local/film-store-v2/repair-candidate-v1 so dependent native compilation/source
review can proceed while the owner finishes residual controls. Runtime remains
owned until explicit handback. A later source repair must use a linked candidate,
never mutate these copies. Parent owns same-reviewer recheck, native integration
and acceptance. No provider/API, personal corpus, installed rc2 mutation, new
release, merge/public push or S2/S3 work is authorized by these changes.
