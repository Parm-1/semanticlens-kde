# Preflight filename correction

The sealed export names `failed-preflight-result.json` and
`failed-preflight-receipt.json` refer to the **repaired, model-free positive**
private GIO association check. They show the owned handler as the only
recommendation after a private `Removed Associations` entry. They are not the
original failed attempt. Their exported bytes and original hashes are unchanged.

`failed-preflight-report.md` describes both the original failure and the later
successful source-opening run; its filename does not classify every observation
inside that report as failed.

[original-failed-gio-result.json](original-failed-gio-result.json) supplies the
actual earlier failed result: `passed:false`, the failed `real owned handler
recommendation` check, and empty event/control lists before service/payload
opening. Its original is
`local/film-open-qualified-preparation-v1/runtime-v1/work/results.json`, SHA-256
`b2f8960a768528074df9cd686ae49b6eb68da3f85917f9185337300cd0a81a41`.
The delivered derivative uses the same literal path substitutions as the export,
SHA-256 `0ada0c1312fe791d1f899fe0796848e5fd7f497b83beaf27048000faf8c30396`. It is an
additional record outside the original 155-file export, covered by SHA256SUMS.

The failure concerns only the isolated test profile's MIME preflight. It is not
a product/opening defect or a successful negative control. The repaired-positive
check changed no personal MIME associations, and the later native opening result
is a separate observation.
