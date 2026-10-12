# Local release candidate assembly

The source and native archives are local candidate artifacts until the user
confirms the public repository identity and publication. The native payload is
the exact tested Arch x86_64 CPU stage: application, service, CLI, embedding
producer, KRunner plugin, model helper, desktop/metainfo and notices. It bundles
no model weights, index, upstream development SDK or system shared libraries.

`packaging/assemble_release_v3.py` uses the pinned public-source export profile
and the existing strict native allowlist/notice validator. It checks stage files
against the original clean-build manifest, traces installed license/notice bytes
to the selected source, and reads every archive member back. It permits only
explicit documentation/evidence/assembler changes after the native build;
changes to compiled source, CMake or the runtime recipe require a new build.
The separately installed metainfo is traced to the selected source and staged
through the unchanged CMake install recipe; its updated homepage does not change
the tested executable bytes. The receipt lists those metadata changes rather than
claiming that different commits were identical.

The assembly helper is an internal publication-preparation tool: it consumes
the pinned internal Git tree before the public repository is initialized.
A normal exported checkout builds using BUILD.md without that internal history.
For new output directories and immutable full commit IDs:

```sh
python3 packaging/assemble_release_v3.py \
  --commit COMPLETE_SOURCE_COMMIT \
  --build-commit 5beacaebb65427478e7e2dd820c8cbea64cf13ea \
  --stage local/clean-arch-v1/root.x86_64/public-stage-v3/stage/usr \
  --stage-manifest evidence/metadata-stage-v1/raw/stage-manifest.json \
  --output local/release-candidate --version 0.1.0-rc1
```

Repeat to another fresh directory and compare both archive hashes. The source
archive uses `kde-index-source/` as its top directory; the native archive contains
`kde-index-0.1.0-rc1-native/usr/`. The commit supplies the deterministic timestamp.
The public source is an explicitly described subset: four unnecessary copied
system binaries are omitted, while original historical manifests remain
unchanged. See [PUBLIC_SOURCE.md](PUBLIC_SOURCE.md). Start the approved public
repository from the exported tree, keeping the internal Git history private.

Build/install instructions are in [BUILD.md](BUILD.md). The corrected source
build, actual `/usr` installation/default plugin discovery/removal, exact
producer parity, storage review and native preview checks have separate original
receipts; assembly does not replace them. The CPU backend has recorded
SSE4.2/F16C/FMA/BMI2/AVX/AVX2 compiler requirements. `GGML_NATIVE=OFF` does not
promise compatibility with every x86-64 processor. Qt/KDE and extractor versions
are recorded in the clean-build evidence; other distributions and GPU backends
remain unvalidated. Model setup is an explicit 864,676,480-byte verified download.

CI prepares a clean native CPU build and runtime-only stage from the documented
recipe. Hosted GitHub CI runs require the approved repository; local checks and
clean-build evidence are available before publication. No archive or receipt
is itself public publication or a general privacy/license compliance guarantee.
