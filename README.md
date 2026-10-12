# KDE Index

An independent, local-first search application for KDE Plasma 6.

Find local documents, PDFs and images by describing what they contain, with
exact-text search alongside semantic matching. A separate local service supports
the native app and KRunner while leaving KDE's existing Baloo index in place.

The first public release candidate is being prepared locally. The Linux CPU embedding producer passed independent
reproduction of upstream text/image references. On a frozen, authored corpus,
KDE Index retrieved the relevant file among the first five results for 20/20
semantic queries, versus 0/20 for Baloo's identical typed queries. All six unique
identifiers ranked first. All three queries with no relevant file still returned
irrelevant suggestions; scores are not confidence estimates. The corrected candidate repeats those totals, with 47 ms warm-query p95 on
the authored corpus. See the [benchmark and its limits](docs/BENCHMARK.md).

The corrected public source snapshot built from scratch in an isolated Arch
userspace. Its actual `/usr` installation passed app searches, standard KRunner
plugin discovery, simultaneous clients and exact removal. See the
[installation evidence](evidence/public-install-v2/report.md) for its scope.
Native PDF previews and ordinary stock KRunner rows have rendered readback checks
on the target Wayland desktop; open/reveal checks identify the original source.
The [storage/status repair review](evidence/combined-storage-review-v2/report.md)
covers local-storage admission, scan bounds, incomplete-index warnings and
preservation during temporary failures. GPU acceleration is not enabled or
validated. KDE Index is a working name, not official KDE affiliation.
The stock tooltip's automatic HTML fetch has an
[independently reviewed adapter repair](evidence/runner-privacy-review-v1/report.md).

Use the [development build instructions](docs/BUILD.md). The app indexes only
selected folders; model setup is an explicit, verified download. No account or
remote query inference is required.

See [the project goal](GOAL.md) for scope, acceptance criteria, architecture,
ownership and the next implementation step.

Inspired by [DigUp](https://github.com/ARahim3/DigUp). Original application code is
GPL-3.0-or-later; incorporated upstream work retains its own license.
See [attribution and model terms](THIRD_PARTY_NOTICES.md). A deterministic
[runtime-only candidate package](docs/RELEASE_V3.md) includes the retained
notices and excludes model weights and indexes. Final packaging and public
repository identity remain pending.

Synthetic assets and upstream reference snapshots retain their separate
[provenance and license notices](docs/ASSET_PROVENANCE.md).
