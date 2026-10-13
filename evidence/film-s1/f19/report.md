# F19 focused native positive: durable manifest job error in status dialog

## Result

The production `FilmPage` status dialog displayed the full nonempty error of a real durable manifest-import job. This closes the missing F19 positive for one job-error path: manifest schema validation. It does not qualify collection-warning rendering or other error types.

## Contract and method

The copied primary producer source records the lifecycle: `film_import_manifest` validates request fields and inserts a pending job (`store.cpp`, `newJob` and operation dispatch); `FilmStore::process` snapshots and parses the sidecar, then validates `film_manifest`; its exception handler calls `finish(..., "failed", message)`, which persists `state` and `error` to the jobs row. `FilmStore::status` reads these rows back. The schema source requires `schema_version`, so a valid JSON object `{}` deterministically reaches durable schema validation rather than failing as an immediate RPC rejection.

The isolated QtTest helper drove production `FilmBackend::importFile` with a three-byte `{}` fixture in a fresh synthetic collection. The sidecar digest reply succeeded. The service accepted the import as a pending manifest job, then exposed `film_manifest: missing schema_version` under job UUID `c4b58a59-afe0-4ca4-9905-3d44ce793967`. The same UUID, failed state, stage, and error appear in the later raw `film_status` reply and the read-only SQLite query after orderly service shutdown.

The helper opened the real `filmStatusDetails` control with a mouse click. In the dialog's own visual child tree it found exactly one visible row matching `manifest · failed · snapshot — film_manifest: missing schema_version`. Its `Label.textFormat` was `0` (`Qt::PlainText`). The saved native capture visibly contains the complete row in the open dialog.

## Evidence and exact checks

- `run-v1/evidence/run/out/f19-positive-results.json` records the fixture digest, job UUID/error, received import request, accepted pending-job reply, terminal status reply, dialog visibility, exact visible row count, PlainText property, and capture result.
- `run-v1/evidence/run/relay.ndjson` contains 45 raw events: 15 forwarded requests, 15 forwarded replies, and 15 received requests. Import is sequence 12; terminal status is sequence 13 and contains the same job ID/error.
- `run-v1/evidence/sqlite-job-readback.json` records the read-only query and exact persisted row. `run-v1/evidence/run/data/film.sqlite` is the post-shutdown database.
- `run-v1/evidence/run/out/f19-status-dialog-error.png` is the native Qt window capture, visually checked against the exact row.
- `run-v1/evidence/run/run-summary.json` records UI exit 0, focused result pass, service exit 0, empty UI/service process groups, removed service socket, and `closed: true`.
- `run-v1/evidence/dedicated-run-summary.json` records all checked executable/model/runner pins, `WAYLAND_DISPLAY=semanticlens-test`, absent `DISPLAY`, private D-Bus and XDG paths, the virtual KWin command, outer bwrap exit 0, empty outer process group, removed Wayland socket, and `closed: true`.
- `attempts/predecessor-v3/` preserves the prior positive capture and receipts. Its outer runner returned 1 because its copied full-suite runner retained all79-specific success requirements. The v4 focused branch now evaluates only this package's F19 acceptance while preserving the full-path acceptance conditions unchanged in the non-focused branch.

The accepted producer pins are service `a59a737bb7e27bcd593d237e36ab113742f566dadd25418d316f1ffebb28f6e3`, worker `dff1212b64fcfb176ce4a3666bd5e6b825259ad3b46f04a625f3995a9dd836e5`, probe `fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0`, and tokenizer `bca52b5f2945f2959b7493f3cd6d48f0cbce35547d6e215b95b555f643659a1b`. The dedicated receipt includes text/image model and native runner helper/plugin pins. `Main.qml`, `FilmPage.qml`, `film_backend.cpp` and `film_backend.h` match current source; the harness's `backend.cpp` predates optional-worker launch plumbing, while the service is launched explicitly with the pinned worker by this runner.

The status response had zero collection warnings; warning rendering remains unqualified. No film query, embedding or indexing ran. Model/tool token usage is unavailable. The runner measured 9.849 seconds outer elapsed and 8.293 seconds inner elapsed for setup, execution and closure; these are not latency measurements. Evidence is limited to this source build, real service, synthetic fixture and isolated KWin target; it does not establish installed-app startup, all error types, warning behavior, or physical-desktop behavior.
