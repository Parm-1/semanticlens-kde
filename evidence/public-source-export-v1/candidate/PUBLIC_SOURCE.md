# Public source export

The development Git history contains frozen synthetic target evidence. Four
copied system binaries in `evidence/desktop-open-v3/candidate/native-bin/` are
unnecessary to build KDE Index. Keep the complete original evidence internal;
prepare a public source snapshot with `packaging/export_public_source_v1.py`.
The export omits exactly those four pinned files, preserves all other tracked
bytes and modes, and records every retained SHA-256 plus the omitted identities
in a receipt. This does not assert license compliance for arbitrary components.

Use a full immutable internal commit and a new output directory:

```sh
python3 packaging/export_public_source_v1.py \
  --commit FULL_INTERNAL_COMMIT \
  --output local/public-source
```

Repeat to another new directory and compare archive hashes. The normalized tar
has a `kde-index-source/` top directory. Its timestamp comes from the pinned
commit. Archive readback checks every included file and mode against the exact
Git tree. The existing source helper scans retained payloads for bounded secret
patterns and refuses model/index/vendor data paths; those checks are limited
and cannot prove that every kind of private data is absent.

Original evidence manifests remain byte-identical and reference the omitted
system artifacts. The public tree is an explicitly described evidence subset,
not a complete packet that passes every historical manifest. Retained reports,
raw measurements, labels, scores and original identities are unchanged. Full
reproduction of that original receiver environment requires independently
supplied matching system artifacts; none is a build dependency for KDE Index.

Do not publish the internal Git history: adding a deletion commit still leaves
those copied objects in its history. Once the final reviewed source snapshot
is available and repository identity/publication is approved, initialize the
public repository from the extracted export rather than pushing this internal
repository. Record its corresponding internal commit and export receipt. Build,
install and validate that final exported source before calling it a release
candidate. The current exporter proof is preparation, not an approved release.
