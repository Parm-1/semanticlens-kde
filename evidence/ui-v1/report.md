# Native GUI development candidate v1

Outcome: ready for independent lifetime/client review. This does not establish a release-ready installation, clean environment or accepted engine integration. Engine-v2 repair is separately active. Pins are relative to this evidence directory; source, metadata, exact staged executables, raw replies and screenshots are frozen.

The native harness passed ten real-service checks in 10.214 seconds against frozen engine-v1 and the development producer. The staged harness passed the same ten in 11.536 seconds against the staged binaries. Actual selected-root indexing, exact PDF page 2, result-window capture, real Poppler raster, preview-window capture, preview-file removal, immediate query invalidation and cached-result removal after fast deletion were observed. The parent inspected result and preview pixels. A separate staged application captured a text query and exited 0 without a logged error. These are synthetic Plasma/Wayland target controls, not recall or latency distributions.

Qt 6.12.0/KF6 6.30.0 and the current Arch host were used. The stage was under ignored local/stage-v2/usr; no global installation, desktop configuration or personal selected root was changed. Full-build/install producer bytes differ from the previous development producer, so actual installed parity was replayed rather than assumed.

Installed producer SHA256: c532522e19a3619c1b938c91d0e4b1373ae7f07c31fd4383fbb27d993bc0e78f. All eight unchanged upstream raw-dot references pass >=0.999, minimum 0.9997917117943279. Two deliberately incorrect framing/image-budget negatives fail parity; four malformed inputs produce no vectors and subsequent recovery passes. Raw replies/evaluation are frozen here; original inputs remain under evidence/embedding-v1/inputs. The sole installed producer exited 0 after 29.300 seconds with peak child RSS 1341944 KiB, including load, requests and teardown. This is producer resource evidence, not GUI memory or a clean environment.

## Shutdown defect and repair

The first staged harness and application exited 134. Backtraces include QLocalSocket destruction through Backend destruction; heap corruption later also surfaced in icon-theme/TLS destruction. Socket/process callbacks reference members declared after those emitters. Implicit reverse member destruction permits callbacks to access state after destruction.

The explicit Backend destructor disconnects socket/process callbacks while state is alive, stops timers, aborts the socket, terminates its active download helper and cleans its preview. Startup additionally follows QApplication/KIconTheme initialization. To distinguish the changes, an isolated full GUI with the same current startup and an empty Backend destructor was compiled. Both bounded full-GUI controls still aborted with corrupted double-linked list: one during incompatible-identity refusal, one with matching staged service/producer and actual text results. Corrected native/staged harnesses and staged application exited normally. Startup initialization alone therefore does not repair this demonstrated defect. No _Exit or suppressed crash handler was used.

A smaller ASan socket-peer experiment passed with both old and corrected teardown. It does not reproduce or protect against the actual GUI defect. Its experimental source/logs are retained for transparency and it is not a shipped unit test. The actual service/window harness and full GUI capture path are the retained validation controls.

## Review scope and limits

Review source-shaped client reply handling, socket reconnection, QML/backend destruction, process/temp-file lifetime, stale/canceled query phases and explicit-root/model setup. Use readonly candidate, verified existing weights and new owned synthetic data/sockets. The reviewer may not author this candidate. The staged service is a full-build v1-source executable, not an accepted engine-v2 candidate; its exact bytes are pinned here.

Capture shutdown is a validation hook. Ordinary app close disconnects its client and leaves the common service available with its independent producer idle expiry. No file action activation or actual model redownload was performed. Service compatibility/recovery remains under repair: old v1 refusal uses fresh data; explicit incompatible-index wipe/rebuild must be integrated with v2 separately.

Installed KRunner discovery/actions, GUI service failure/restart recovery, large directory responsiveness, clean checkout/environment, transitive notices and final public identity remain pending. No adversarial same-user filesystem sandbox, universal upgrade compatibility or no-match cutoff is claimed.

All named parent test sockets are absent and native tool processes completed. Old GUI controls aborted after owned-service shutdown. No parent test service/producer remains. See closure.json and raw logs. Model/API usage is unknown; native wall/RSS scopes are not billing evidence. Corrections after delivery require a linked successor.
