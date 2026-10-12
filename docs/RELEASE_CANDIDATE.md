# SemanticLens for KDE 0.1.0-rc1

[v0.1.0-rc1 is published](https://github.com/Parm-1/semanticlens-kde/releases/tag/v0.1.0-rc1)
as the first prerelease of [SemanticLens for KDE](https://github.com/Parm-1/semanticlens-kde).
The [hosted clean CPU build](https://github.com/Parm-1/semanticlens-kde/actions/runs/38314493646)
passed. All five GitHub assets were downloaded and matched the local assemblies.
The release tag is `1c26ec8c7f75d95c0a9f24d329952ca4bd29b0b2`; code CI ran on
`df327b084746cfdb33e5845175e80cd4fdd4d514`. Only ten frozen preparation records
were added between those commits; build inputs are unchanged.

The exact runtime passed Linux text/image vector checks, the fixed retrieval
comparison, current lifecycle controls and installed KDE integration:

- Eight upstream vectors, malformed-input/recovery and text-only controls pass.
- Semantic top-five: 20/20; all six identifiers first; all four labeled PDF pages.
- Warm service-query p95: 47.236 ms on the authored 28-file corpus.
- Current edits/rename/delete/exclusion/interruption/restart/idle controls: 74/74.
- Actual disposable Arch `/usr` installation, normal KRunner discovery, overlapping
  app/runner requests, shutdown and exact removal pass.
- Native PDF-page rendering, selected-root boundaries and the repaired tooltip/
  incomplete-index status have reviewed evidence at their stated scopes.

The comparison reuses the frozen Baloo baseline, not a fresh paired run. This
small synthetic corpus does not establish general quality. All three no-match
queries return irrelevant suggestions. GPU operation and other distributions
remain unvalidated. Memory figures are sampled service-tree observations,
excluding GUI/KRunner; no hard RSS or arbitrary storage-provider guarantee.

The source archive includes code, notices, demo corpus, CI and original scoped
evidence. It omits four unnecessary copied system binaries, preserving explicit
historical manifest omissions. The native archive has 21 runtime/notice files;
model weights, indexes and system libraries are excluded. The core service and embedding producer are unchanged; GUI and runner display
branding passed affected native build and label checks. The current metainfo carries the
selected public repository homepage. [RC_AUDIT.md](RC_AUDIT.md) maps the complete goal
to the completed first-release scope and its evidence limits. [BUILD.md](BUILD.md) and
[RELEASE_V4.md](RELEASE_V4.md) describe building and packaging.

The source archive has 2,065 retained files matching the tag's complete public
Git tree, including modes. The native archive has 21 files and 13 installed
notices. Both archive/receipt sets were assembled twice with identical bytes.

| Archive | SHA256 |
|---|---|
| `semanticlens-0.1.0-rc1-source.tar.gz` | `20cbae80a7ec72ae5cd518eb35ed00867b6a71edf75707042215c3a20fd6230f` |
| `semanticlens-0.1.0-rc1-native.tar.gz` | `985a71c9c7052b87b63aa58421353b50691cada840093d5d5a371bd53aae3daf` |

Frozen archives and original evidence preserve their preparation-time status;
this current page records the subsequent verified publication.

The linked [branding evidence](../evidence/semanticlens-branding-v2/note.md)
records the visible name, stable plugin ID and four expected changed stage
files; the other 17 retained bytes are unchanged. Its original broad test
timeout remains preserved. A separate startup investigation reproduces the
relative-socket test configuration failure and confirms actual service/model
readiness with an absolute owned socket; it does not change search semantics.

The [independent startup diagnosis](../evidence/semanticlens-startup-v1/report.md)
explains the retained test timeout without source edits. Absolute-socket startup
reaches observed verifying/ready phases and Backend readiness; no replacement
full-window or indexing claim is made from that narrow run.
