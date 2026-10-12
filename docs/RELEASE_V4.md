# SemanticLens release assembly

The selected project name is SemanticLens for KDE and the public repository is
[Parm-1/semanticlens-kde](https://github.com/Parm-1/semanticlens-kde).

`packaging/assemble_release_v4.py` uses the existing public-source omission
profile, runtime-only allowlist, notice-to-source checks and full archive
readback. It names the assets `semanticlens-VERSION-source.tar.gz` and
`semanticlens-VERSION-native.tar.gz`. The source top directory remains
`kde-index-source/`; the native top directory is `semanticlens-VERSION-native/`.
Executables and local plugin/data IDs remain stable. No weights, indexes or
system shared libraries are bundled.

This internal publication-preparation helper consumes a pinned internal Git
commit and a frozen stage manifest. A normal public checkout builds using
[BUILD.md](BUILD.md), without internal history. Changes to compiled source
require a corresponding validated build commit, not a metadata-only exception.

```sh
python3 packaging/assemble_release_v4.py \
  --commit COMPLETE_SOURCE_COMMIT \
  --build-commit VALIDATED_BUILD_COMMIT \
  --stage PATH_TO_NEW_STAGE_USR \
  --stage-manifest PATH_TO_FROZEN_STAGE_MANIFEST \
  --output PATH_TO_NEW_OUTPUT --version 0.1.0-rc1
```

The output directory must be new. Repeat into another new directory, compare
archive hashes and read back every member against the receipts. The public
source remains the declared subset omitting four copied system binaries;
original historical evidence manifests stay unchanged. See
[PUBLIC_SOURCE.md](PUBLIC_SOURCE.md) and [ASSET_PROVENANCE.md](ASSET_PROVENANCE.md).

The name change is restricted to display strings and package metadata. Affected
GUI/runner builds and metadata are validated separately; unchanged service,
producer, model, ranking and lifecycle evidence retain their original scope.
The runtime remains the tested Arch x86_64 CPU stage with its recorded instruction
and system-library requirements. Repeated packaging is a byte-reproducibility
claim, not a claim of bit-identical builds on arbitrary toolchains.

Publication is a separate operation. Release download hashes, tag identity and
hosted CI must be checked after publishing; local receipts alone cannot prove it.
