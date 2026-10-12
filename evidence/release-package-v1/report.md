# Local package assembly v1

Status: checked development candidate package; not a release candidate accepted
for publication. No public repository identity or publication was selected.

## Candidate identity and inputs

- Source archive commit: `4d7d167ddc05377fa35dd5015f97e38de2f2b4fb`.
- Native build commit: `8123e0769645c533de996866c3ec35f09e270793`. Source changes
  after that build commit are limited to documentation and frozen evidence.
- Native stage: `local/installed-host-v3/stage/usr`, all 54 files matched the
  `actual_new` hashes in `evidence/clean-build-v4/stage-comparison.json`.
- Stage-comparison SHA256:
  `5ca74e1a5ca6c8d54489c509ba771825c541fbc8d71ea354288deff3326864ea`.
- Clean-build-v4 report SHA256:
  `c8c4661d475c2e81819eedf118e5b00f35089aa56e8703dc0710030a0fd909ec`.
- Clean-build-v4 pins SHA256:
  `f3eb5096ac07124e36dbd2f89d53cc21aba48980ef8fec5fce4941711cb945e8`.
- `SOURCE_DATE_EPOCH=1791786444`, derived from the source commit timestamp.

The pinned native module is `a626737f2e6988437cc2c15d25bffcd76cdfc780588557033ef4efc8dc3fb32d`.
The staged producer, service, and GUI hashes remain respectively
`3dd4ddd9a133e9861ec834a2434121f99872b2ac94b1b211690a620357408414`,
`3176fa48374a69e35b2b6b064e68dc86baf852dfd73d49c66716c569fe575845`, and
`7362440ed0d4df35f1b8a8a58121db08a349cfe199f2849a1d22c7bd252f56d9`.

## Assembly and readback

`packaging/assemble_release.py` produced source and native archives in two fresh
output directories with identical inputs and epoch. Each run checked the pinned
stage inventory, exact license-copy equality, source/build commit relationship,
and the archive contents after writing. All 1,134 source files read back with
the Git object identity from the selected source commit. All 54 native files
read back with their stage SHA256 values. The package includes all 13 staged
license/notice files. The staged tree has the project KRunner module and no
operating-system shared-library payload.

Archive hashes matched across both assemblies:

- `kde-index-0.1.0-dev-candidate1-source.tar.gz` —
  `dec5fbbbc551936730b692e7defd64bb5b97f590dd030d7ed67d922b49ded1cb`
- `kde-index-0.1.0-dev-candidate1-native.tar.gz` —
  `8004fa0d43be459cd8a84912d2605e385b055148bf26e1b32c792784a4c9e55d`

The locally retained archives are under ignored `local/release-package-v1-final/`;
the second assembly is under `local/release-package-v1-repeat-final/`. The source
archive is 27 MiB and the native archive is 7.7 MiB. Only the compact receipt,
archive checksums, and this report are retained in Git evidence.

## Privacy and notices

The source archive is the full tracked Git snapshot, including frozen synthetic
validation material and historical build/review logs. A bounded scan checked
1,134 files against four common private-key/cloud/API-token patterns (4,536
file-pattern checks) and rejected model-weight, index, `.env`, and vendored
runtime paths. No matches were found. This does not prove the absence of every
possible secret or personal datum.

There are `/home` path strings in 106 source files: 100 contain this workspace's
path, two contain synthetic retrieval-fixture home paths, and four contain
other workspace-local build/runtime subpaths. These are path references in
logs/fixtures, not indexed personal-folder contents. The project benchmark and
native-stage evidence use synthetic/disposable inputs. Frozen evidence was not
edited or redacted. A public source archive will need a fresh privacy review
after the final source snapshot is chosen.

The source snapshot retains the runtime fetch/patch recipe, GPL-3.0-or-later
application license, DigUp and llama.cpp attribution, and all listed CPU
dependency notices. The staged native archive includes the corresponding
license files. Model weights are absent and remain a separate explicit user
download; no model inference, download, API call, or billing action occurred in
this packaging work.

## Limits and closure

This validates deterministic local archive assembly and its source/stage
identity. It does not validate a clean rebuild from the archive, physical
desktop presentation, normal KRunner result layout, file opening, simultaneous
clients, or a system installation/uninstallation. The native stage is the
current Arch/Qt 6.12/KF 6.30 CPU build and is not a portable binary claim. The
normal KRunner layout/open checks and final install path remain pending with
the parent-owned target work.

Both assembly processes exited successfully. Their temporary directories were
renamed into the two ignored output directories; no assembly process, native
desktop session, model worker, Baloo workload, or background handle remains.
Input stage/evidence files were read only. No commit, install, configuration
change, or external publication was performed.

The measured scope was two complete local archive assemblies and their embedded
readbacks. No model weights, model inference, remote API, or billing operation
was used. No usage-meter or elapsed-time receipt was available, so model/tool
usage and exact wall time for this package remain unknown.
