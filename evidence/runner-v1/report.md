# KRunner adapter v1

Implemented a native KF6 KRunner plugin with explicit `find ` syntax. It queries the documented local service socket using JSONL protocol v1, checks response protocol and request identity, bounds socket waits and response size, and drops work when its runner context becomes invalid. A canceled request closes its per-request socket, matching the service contract's disconnect cancellation. It maps file title, passage and PDF page to native results, maps normalized dot score monotonically into KRunner relevance, and exposes open/reveal behavior. When the service is unavailable it displays a setup result that launches `kde-index` if installed.

Validation: standalone CMake configure/build passed. A native KPluginFactory smoke loaded the built shared module and metadata, then exchanged a synthetic search with a QLocalServer and verified title, passage/page, URL and relevance mapping. Full log is `smoke.log`; pinned source/module hashes are `SHA256SUMS`.

Limits: service producer was not available during validation, so this does not establish compatibility with the implemented service binary. No live Plasma KRunner session, installed plugin discovery, desktop app action, Okular page launch, or cancellation race was exercised. No private roots or desktop configuration were touched. Smoke fixture thread joined and its socket was removed.
