# Service watch-install reconciliation v1

Status: author package READY for the parent’s independent combined core/service review. The bounded repair is only in `src/service-v2/main.cpp`; the accepted `src/service/main.cpp` and `src/core/engine.cpp` are unchanged. No parent CMake, Engine, UI, root policy, IPC, ranking or cancellation contract was changed.

## Settled source boundary

The prior discriminator at `local/build-own/local-root-v1/repair-v2/watch-gap/results.json` (SHA256 `60a8ed6343f2c2915fc7aee3df66dddebe43926f81126c589367564cbf3e5199`) witnessed the selected root absent from the service’s actual `/proc/<pid>/fdinfo` inotify records at file creation and present afterward. The pre-explicit-scan index had one file; the subsequent explicit scan returned `auto.txt` with its actual body. The prior result is preserved and was not edited.

The service source polls `Engine::watchPaths()` every 500 ms, adds paths through `QFileSystemWatcher`, and had no reconciliation when an added watch first became available. `Engine::watchPaths()` is the engine’s published watch set (`src/core/engine.cpp`, SHA256 `b997ee10c5211476fccf985100adee61b9733194a3deaa023aba09e39024ddb3`). The repair checks the watcher's resulting installed path set after `addPaths`; only if one of the newly requested paths is present does it call the existing `schedule()` function. That function sets the shared `rescanPending` flag and starts the existing 350 ms single-shot debounce. Its existing timeout defers while the engine is not ready, busy or paused; the 500 ms sync callback retries the pending debounce after busy work ends. A failed batch with no new installed path schedules nothing. Once the path is already watched, the add list is empty, so this hook cannot self-trigger another reconciliation. This preserves service-owned scan serialization and coalescing.

The source delta is five lines at the existing watch-install point. Native watch presence was independently read from real `/proc` inotify fdinfo records keyed to each controlled root inode; it was not inferred from `addPaths` return flags.

## Candidate and build identity

`candidate/` freezes both matched service executables, the new service source and the root-relative CMake build description. The existing engine source and original service source are pinned by their repository hashes in this report and remain unchanged. Candidate executable hashes are:

- Original service: `717f97ce74e8c74e45821e6271e58ecb457b281acdb8a5815136d7dbf73e6eb1`
- Service-v2: `57d2156f751f2cc830757225d9a6884dbb3fb255af05cc979de2436dd4e580bd`
- New service source: `d8f9a7bc1f0c0caa5ea9ddc2dea5e3302db166f4152094b2b506c6250c1aa5cf`

Both binaries were built from `candidate/CMakeLists.txt` against the unchanged accepted `src/core/engine.cpp`; `cmp` confirmed they are byte-identical to the binaries used by the integration helper. Build command:

```sh
cmake -S evidence/service-watch-v1/candidate -B local/build-own/service-watch-v1/frozen-build -DCMAKE_BUILD_TYPE=Release
cmake --build local/build-own/service-watch-v1/frozen-build -j2
```

Producer and model inputs were the already accepted local artifacts: CPU producer SHA256 `3dd4ddd9a133e9861ec834a2434121f99872b2ac94b1b211690a620357408414`; text model SHA256 `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`; image model SHA256 `c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f`. The run uses CPU with two threads. These inputs establish the source retrieval run identity only; this package makes no new serialized-vector parity, GPU, benchmark or game claim.

## Matched actual-inotify control

`candidate/tools/watch_install_reconcile_v1.py` (source helper: `tools/watch_install_reconcile_v1.py`) runs each binary against a fresh private synthetic root, private engine data and socket. In both cases it adds the root, records actual fdinfo immediately before creation, creates the same file/body, then records the resulting watcher and service state. The original is bounded at four seconds for automatic indexing; only that baseline then receives an explicit scan. The candidate is observed up to 25 seconds and receives no explicit scan. Both then remain available for a 10-second idle status interval and are explicitly shut down.

The frozen `results.json` contains two matched cases, four raw fdinfo watcher snapshots, six final status snapshots, three search replies, and the original's explicit-scan response. The decisive observations are:

- Both roots had no native inotify watch at the creation endpoint and had one afterward.
- Original service remained at indexed count/revision `1/1`; its pre-scan query did not return the new body. Its explicit scan advanced to revision 2 and returned the exact `auto.txt` path and complete controlled body.
- Service-v2 reached indexed count/revision `2/2` without an explicit scan. Its search returned the exact `auto.txt` path, literal match and controlled body. Revision stayed `2` across the subsequent 10-second idle observation, and status remained available.
- Both service processes exited with status 0 and both owned sockets were absent after shutdown. No child process was present at the final idle/shutdown observation. The helper does not instrument per-request producer process lifetime or count inference invocations; stable revision and the source path establish no repeated indexing in this interval, while an inference-count claim remains unmade.

Replay the exact frozen executables and helper from the repository root with:

```sh
python3 evidence/service-watch-v1/candidate/tools/watch_install_reconcile_v1.py
```

The helper uses the frozen executables in `candidate/bin/`; the build reproduction above recreated byte-identical executables (`cmp` passed). Original raw results are `results.json`, with all payload hashes in `pins.sha256`.

## Scope and handoff

This repairs only the observed file-created-before-watch-install gap. It does not claim general watcher completeness, protection against files changing during scan, successful watch installation under native capacity exhaustion, UI status behavior, integration with `core-v3`, or target desktop acceptance. Parent owns CMake/Engine integration and the assigned independent combined core/service review. Any affected repair should be a linked successor, preserving this package.

No billing, token-use or cumulative package wall-time measurement is available. The raw helper records monotonic endpoint timestamps for its controlled intervals; these are not aggregate implementation or usage accounting. The local model files and producer were read-only inputs. The helper's synthetic roots, data, sockets, and service processes were private and owned; the sockets and processes were closed at delivery. No personal roots, Baloo settings, host installation, mount, network model download, or Wayland resource was used.
