# Local package assembly v2

Status: checked local development-candidate assembly, not a public release or accepted RC. No publication, system install, or identity choice was made.

## Inputs and identity

- Source archive and native build commit: `c394cca550d76b7754ffbbe41bc579b202b0c9ff`.
- Source commit timestamp: `SOURCE_DATE_EPOCH=1791788062`.
- Stage: `local/clean-arch-v1/root.x86_64/checkout/local/stage-v5/usr`.
- Stage manifest: `evidence/clean-build-v5/stage-comparison.json`; the clean-build-v5 pin set validates its report, source identity/status, build log, observation and stage manifest.
- Native files matched the manifest exactly: 21 files. Their bytes are unchanged from the retained runtime files in stage-v4. The v5 comparison records removal of 33 upstream SDK files (headers, static libraries, CMake package files and pkg-config metadata).
- The v2 allowlist contains four binaries, the KRunner module, model-download helper, desktop/metainfo entries and 13 notices/licenses. The 13 notice/license bytes match the selected source commit. No model weights, indexes, vendored runtime, credentials, or operating-system shared libraries are included.

## Assembly and readback

`packaging/assemble_release_v2.py` assembled into two separate ignored local output directories. Both runs checked the same full source/build commit, the stage manifest, exact source-tree Git blob IDs, source notice correspondence, the strict runtime-only native allowlist, and native archive readback hashes. Each archive checksum was verified with `sha256sum -c`.

Both runs produced identical hashes:

- `kde-index-0.1.0-dev2-source.tar.gz`: `704f6002d88b374e4b82ae6243b28d0b605fb053c9a484dfa4845d0f6783987e`
- `kde-index-0.1.0-dev2-native.tar.gz`: `db5cae2c058f2e2eebec1b5bd7da4892b1dcfdee48f280effc38a50abfb11c93`

The source readback contained all 1,237 tracked blobs at the source Git identities. Native readback matched all 21 staged files. `package-receipt.json` records per-file stage hashes, archive hashes, 13 notices/licenses, and bounded scan counts. The four-pattern credential/private-key scan found no matches; it does not prove the absence of every possible secret or personal datum.

The source archive preserves frozen synthetic validation evidence and historical logs. It has 112 files containing `/home/` path strings; 105 contain this workspace path and seven contain other home-style references in frozen review, fixture, or build evidence. These are retained path references, not evidence that personal folders were indexed. No frozen evidence was edited or redacted. The old v1 package files and report are unchanged.

## Build and validation boundary

The v2 source/documentation/evidence additions are not in the c394 proof archive. The parent is independently checking that c394-produced source archive in a fresh userspace. Final packaging must reassemble from the complete later source commit and a stage built from that exact commit. This v2 assembly does not claim a source-archive rebuild pass.

The staged ggml CPU variant was compiled with `-msse4.2 -mf16c -mfma -mbmi2 -mavx -mavx2`; the recorded CMake cache has `GGML_NATIVE=OFF`. This build therefore requires those CPU instruction features for its CPU variant. No minimum processor model or generic x86-64 portability has been established. CPU behavior is validated only on the recorded environment; GPU and broader CPU support remain unvalidated.

The archive’s `usr/` layout is not an installation test. No install or uninstall was run. The supported source recipe, explicit model-download flow, selected-root behavior, and exclusions are documented in `docs/RELEASE_V2.md` with existing BUILD, DEPENDENCIES and ENGINE references. The model remains a separate explicit download; it is not packaged.

Normal stock KRunner result readback and simultaneous-client checks remain qualified only at their original recorded scopes. Physical desktop presentation and completed file opening remain pending. The private fresh-profile portal observation reached KDE AppChooser and did not observe a launched receiving application or loaded source; this does not establish the user's default handler behavior. No global desktop, MIME, KRunner, or Baloo setting changed.

Public repository/developer identity and metainfo URL remain owner decisions. No publication, model download/inference, API call, system installation, or global configuration change occurred. Both assembly processes and their temporary directories closed successfully. Usage-meter data and exact elapsed time for this package were unavailable and remain unknown.
