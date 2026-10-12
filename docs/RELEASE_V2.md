# Local release-package v2

This document describes a reproducible local development-candidate assembly for the runtime-only native stage. It is not a public release, portable binary, or accepted release candidate. The v1 proof remains historical and unchanged; its native payload had 54 files, including upstream SDK material. V2 consumes the corrected clean-build-v5 stage: 21 runtime and notice files, with no upstream headers, static libraries, CMake package files, or pkg-config files.

## Candidate inputs and assembly

The frozen v2 proof uses source and native build commit `c394cca550d76b7754ffbbe41bc579b202b0c9ff`, clean-build-v5's stage comparison, and its Arch x86_64 userspace stage. `evidence/clean-build-v5/pins.sha256` pins the build receipt and stage manifest. The stage manifest records each installed file's SHA-256. The assembler also checks the exact source Git tree, requires the build commit to be an ancestor of the source commit, traces staged license and notice bytes to that source commit, and permits only the runtime paths plus notices found under the source `LICENSES/` directory.

For a fresh output directory, derive the archive timestamp from the selected commit and run twice to distinct output paths:

```sh
commit=c394cca550d76b7754ffbbe41bc579b202b0c9ff
epoch=$(/usr/bin/git show -s --format=%ct "$commit")
export SOURCE_DATE_EPOCH="$epoch"
python3 packaging/assemble_release_v2.py \
  --commit "$commit" --build-commit "$commit" \
  --stage local/clean-arch-v1/root.x86_64/checkout/local/stage-v5/usr \
  --stage-manifest evidence/clean-build-v5/stage-comparison.json \
  --output local/release-package-v2 --version 0.1.0-dev2
python3 packaging/assemble_release_v2.py \
  --commit "$commit" --build-commit "$commit" \
  --stage local/clean-arch-v1/root.x86_64/checkout/local/stage-v5/usr \
  --stage-manifest evidence/clean-build-v5/stage-comparison.json \
  --output local/release-package-v2-repeat --version 0.1.0-dev2
```

Compare both archive hashes and verify each `SHA256SUMS` from its output directory. The helper refuses an existing output directory. Its source archive is a normalized Git archive and its native archive is a normalized `usr/` tree. It reads both archives back: every tracked source blob must match the pinned commit and every native file must match the stage manifest. The evidence receipt records file counts, hashes, a bounded credential-pattern scan, and source-path reference counts. Such pattern scans are a limited check, not proof that arbitrary private data or every possible secret is absent. Frozen synthetic validation evidence remains in the source archive and is not changed or redacted.

The checked-in v2 helper and documentation are newer than the `c394...` proof input. The proof therefore does not include these new files. The final assembly must use the eventual complete source commit and a native stage built from that commit, then repeat the clean archive rebuild/readback against that final identity. The parent is separately checking the current c394 source archive in a fresh userspace; this document does not claim that check passed.

## Build, model, and native scope

Use [BUILD.md](BUILD.md) and the pinned runtime setup recipe to build from source. The recipe fetches the pinned upstream runtime source and applies the recorded patch; it does not require the repository root's Git metadata to build the application, though fetching that runtime uses Git. The model is not in either archive. Download is an explicit user action after reviewing the linked model card and terms; the two model files total 864,676,480 bytes and are size- and SHA-256-verified. They stay outside Git and outside the native payload. The engine selects no roots by default; users explicitly choose roots and can configure root-relative exclusions. Do not point validation at personal folders or Baloo's index.

The native payload is the Arch x86_64 CPU build. Its existing configure receipt says `GGML_NATIVE=OFF`, but the emitted ggml CPU backend variant still uses `-msse4.2 -mf16c -mfma -mbmi2 -mavx -mavx2`. Thus this build requires those CPU instruction features for that backend variant. The evidence does not establish a minimum processor model or compatibility with every x86-64 machine. `GGML_NATIVE=OFF` is not a generic-x86 portability guarantee. CPU inference is the validated foundation on the tested environment; GPU operation and broader CPU portability are unvalidated. System shared libraries come from the target operating system and are not bundled.

## Install and desktop validation boundary

The archive lays out native files under `usr/`; extraction alone does not qualify installation. No system install or uninstall has been performed or validated by this package proof. A release operator must test the documented staged install path in a clean userspace and verify both installation and removal there before claiming those behaviors.

The retained integration evidence qualifies normal stock KRunner row readback and simultaneous outstanding clients at their recorded scope. Physical desktop presentation and completed file opening remain pending. The current fresh-profile portal observation reaches KDE's AppChooser because the private profile has no prior text/plain choice and no recommended handler; it did not observe a launched receiving application or loaded source. A default handler alone is not evidence that the chooser will be skipped in a user's profile. No host MIME, KRunner, Baloo, or desktop configuration was changed for this packaging proof. See the frozen `evidence/desktop-boundary-v2/report.md` and `evidence/installed-integration-v2/report.md` for the original observations and their limits.

The public repository identity, final developer identity, and metainfo URL still require the owner's choice. These archives are local development artifacts only; no public upload or release claim follows from deterministic assembly.
