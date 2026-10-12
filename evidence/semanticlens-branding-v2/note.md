# SemanticLens branding evidence v2

This portable successor links the display-branding candidate and stage from [v1](v1/report.md) with its small native readback materials. The copied v1 report, checks, source hashes and 21-file stage manifest are byte-identical to `evidence/semanticlens-branding-v1/` at packaging time. The new raw receipts and probe sources are under `raw/`; every included asset is pinned in `pins.sha256`.

The staged app capture and native Qt label probe read `SemanticLens for KDE`; the KPluginMetaData readback reports that label with the unchanged `kde-index-runner` ID. The screenshot is a native window capture using an empty model directory and disposable XDG paths. The label probe source/CMake is included so a consumer can inspect exactly how the native QML title and application display name were read.

The original UI/service helper result remains a generic 90.032-second `completion-within-bound` failure. Its helper waited for `Backend::modelReady()` before indexing, but the preserved raw output does not contain a phase, Backend message, connection/launch observation, or raw service readiness response. The isolated model-path receipt shows that both expected model files were present at `/models`; therefore missing model files are ruled out, while the exact service-start/readiness versus backend-connection cause remains unresolved. The timeout does not identify which phase failed. No broader product or UI-service claim is made from that result.

No model weights, index database, build tree, or staged binary is included here. The stage's per-file hashes remain in `v1/stage-manifest.json`; consumers need the separate owned stage path recorded in `v1/checks.json` to inspect those bytes.
