# KDE Index

**Private semantic file search for KDE Plasma 6, powered by Google EmbeddingGemma 2.**

Find documents, PDF pages and images by describing what you remember. Semantic
matching works alongside exact-text search, so both “instructions for cleaning
the cooling system” and an identifier such as `PDF-10299` can lead to the right
file. Search runs locally and complements KDE’s existing Baloo index.

## Features

- Native Qt 6/Kirigami interface with passage, PDF-page and image previews.
- KRunner integration: type `find ` followed by a query.
- Text/Markdown, PDFs, JPEG and PNG; PDF and screenshot OCR through Tesseract.
- Selected folders only, with exclusions, pause/resume and index removal.
- Incremental indexing for changed, renamed and deleted files.
- Open results in their associated application or reveal them in Dolphin.

## Local AI and privacy

[Google DeepMind’s EmbeddingGemma 2](https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md)
creates text and image embeddings through a pinned llama.cpp runtime. A local
service combines those vectors with SQLite exact-text search and serves the app
and KRunner. The current candidate uses **CPU inference**; GPU acceleration has
not been validated.

Queries, indexed content and embeddings stay on the machine. There is no account
requirement or telemetry. Model weights are a separate, explicit download
(about 865 MB), verified by size and SHA256. Review the
[model license and policy](THIRD_PARTY_NOTICES.md) before downloading.

## Build and run

The supported target for this release candidate is **Arch Linux with KDE Plasma 6
and Wayland**. Install the dependencies listed in [BUILD.md](docs/BUILD.md), then:

```sh
git clone https://github.com/Parm-1/kde-index.git
cd kde-index
./tools/runtime_setup.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
./build/bin/kde-index
```

In the app, download the model and select the folders to index. See the
[installation instructions](docs/BUILD.md) for staging the app and KRunner plugin.

## Release status

**v0.1.0-rc1 is being prepared for publication.** The
[hosted CPU build and installation checks](https://github.com/Parm-1/kde-index/actions/runs/38310540856)
have passed. Source and Arch CPU runtime archives will be published through
[GitHub Releases](https://github.com/Parm-1/kde-index/releases).
The native archive requires compatible system libraries and the recorded CPU
instructions; it is not a self-contained installer. Other distributions and GPU
backends remain unvalidated.

On a fixed, authored 28-file corpus, semantic top-five recall was 20/20, compared
with 0/20 for the frozen Baloo baseline. All six unique identifiers ranked first;
warm service-query p95 was 47 ms. This small synthetic comparison does not
establish general search quality. All three no-match queries returned irrelevant
suggestions, so result scores should not be read as confidence estimates.
[Benchmark details](docs/BENCHMARK.md) and the
[release audit](docs/RC_AUDIT.md) document the evidence and its limits.

## License and acknowledgments

Original application code is **GPL-3.0-or-later**. Incorporated upstream work
retains its own licenses; see [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Inspired by [DigUp](https://github.com/ARahim3/DigUp), with local inference powered
by Google EmbeddingGemma 2 and llama.cpp. This is an independent project, without
official KDE or Google affiliation.
