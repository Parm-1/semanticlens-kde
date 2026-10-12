# Local development candidate packages

This workflow assembles a reviewable source archive and the exact staged Arch
CPU payload. It does not publish either archive or qualify a user installation.
The current package is a development candidate: normal KRunner result layout,
file opening, simultaneous clients, physical desktop presentation, and the
final clean-install path still need target evidence. The working name and
metainfo identifier do not establish a KDE affiliation or public repository.

## Assemble the pinned candidate

The first candidate uses source commit
`4d7d167ddc05377fa35dd5015f97e38de2f2b4fb`. Its native stage was built from
`8123e0769645c533de996866c3ec35f09e270793`; the intervening source changes are
documentation and frozen evidence only. The stage is
`local/installed-host-v3/stage/usr`, pinned by
`evidence/clean-build-v4/stage-comparison.json`. That comparison records all 54
installed files. The runner module is the v4 successor; the app, service,
producer, metadata, and notices retain their previously checked byte identities.

From the repository root, choose a new output path. `SOURCE_DATE_EPOCH` is
required and must be the same for repeated assembly. This example derives it
from the pinned source commit:

```sh
export SOURCE_DATE_EPOCH="$(/usr/bin/git show -s --format=%ct 4d7d167ddc05377fa35dd5015f97e38de2f2b4fb)"
python3 packaging/assemble_release.py \
  --commit 4d7d167ddc05377fa35dd5015f97e38de2f2b4fb \
  --build-commit 8123e0769645c533de996866c3ec35f09e270793 \
  --stage local/installed-host-v3/stage/usr \
  --stage-manifest evidence/clean-build-v4/stage-comparison.json \
  --output local/release-package-example-output \
  --version 0.1.0-dev-candidate1
```

The output path must not already exist. The helper checks the source/build
commit relationship, requires code identity across that interval, compares the
stage against every pinned file hash, checks license copies, and reads both
archives back. It normalizes archive ownership and timestamps, then writes
`package-receipt.json` and `SHA256SUMS` next to the two archives. Repeat into a
second new output directory with the same inputs and epoch to check archive
hash determinism. Keep generated archives under ignored `local/`; keep only the
compact receipt and report in `evidence/release-package-v1/`.

The source archive is the original tracked Git tree at the declared source
commit. It retains frozen validation evidence, including synthetic fixtures and
workspace-local build/review logs; it is not a minimal source-only export. The
bounded assembly scan rejects model weights, indexes, `.env` files, vendored
runtime directories, and common private-key/cloud/API token patterns. A scan
cannot prove that every possible secret or personal datum is absent. In this
snapshot, workspace path strings occur in frozen logs and retrieval fixtures;
the retrieval review's `/home` path examples are fixture locations. No personal
folder corpus is part of the frozen benchmark or the native stage. The scan
does not rewrite or redact frozen evidence.

The source archive retains the pinned runtime setup script, exact llama.cpp
revision/patch reference, DigUp attribution, and source licenses. The native
archive contains the complete 54-file staged `/usr` tree, including the app,
service, CLI, CPU producer, KRunner module, desktop/metainfo entries, model
download helper, and all staged license notices. It contains no model weights,
index, operating-system shared libraries, or machine-local configuration.
System libraries, Qt, KDE Frameworks, Poppler, and Tesseract remain operating
system dependencies as described in `BUILD.md` and `DEPENDENCIES.md`.

## Stage, install, and remove

The validated local operation is CMake staging from `docs/BUILD.md`:

```sh
DESTDIR="$PWD/local/stage" cmake --install build --prefix /usr
```

For a later, explicitly accepted system install of a final candidate, the
native archive layout supports extraction into `/`:

```sh
tar -xzf kde-index-<version>-native.tar.gz -C / --strip-components=1
```

Archive extraction is not a package-manager transaction and does not establish
KRunner discovery, normal result layout, file opening, or a qualified desktop
installation. Do not use this development candidate as a system install while
those target gates remain open. A future distribution package should own file
conflicts and upgrades through its package manager.

For an extraction-only install made from an accepted archive, remove only the
regular files listed by that same archive. This leaves directories in place and
does not remove unrelated files:

```sh
archive=kde-index-<version>-native.tar.gz
prefix=kde-index-<version>-native
while IFS= read -r member; do
  case "$member" in
    "$prefix"/usr/*) sudo rm -f -- "/${member#*/}" ;;
    "$prefix"|"$prefix"/|"$prefix"/usr|"$prefix"/usr/) ;;
    *) printf 'Unexpected archive path: %s\n' "$member" >&2; exit 1 ;;
  esac
done < <(tar -tzf "$archive")
```

Do not use that removal procedure after another package manager has taken
ownership of the same paths. No system installation or removal was performed
for this candidate.

## Candidate release sequence

When the remaining target checks and the documented install path are accepted,
reassemble from the complete candidate source commit and its matching stage.
Use a new version label/output directory and the final source commit timestamp
as `SOURCE_DATE_EPOCH`. Recheck source/build correspondence, native stage file
identities, archive readback, and independent archive hashes. Keep the
archives out of Git; preserve the compact report, receipt, and checksums as
evidence. Then prepare release notes and dependency/model policy references for
review. Set the final project/repository identity and publish only after the
user confirms those choices and publication.
