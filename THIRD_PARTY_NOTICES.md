# License and attribution

Original KDE Index application, service, client and integration code is licensed
under GPL-3.0-or-later, unless a file carries another notice. See `LICENSE`.
This choice keeps the distributed native application compatible with its KDE/Qt
dependencies. It does not change the licenses of incorporated upstream work.

The CPU probe incorporates pipeline/preprocessing code from **DigUp**, copyright
2026 Abdur Rahim, MIT, commit `6faa29e4c315a3a591de1b41bd78751bdf39588d`.
Its original notice is in `probe/NOTICE` and `LICENSES/DigUp-MIT.txt`.
Reference inputs and source snapshots in `evidence/embedding-v1` preserve that
attribution. The llama.cpp no-logits patch is also from DigUp.

**llama.cpp**, commit `4d756bc72bf00a4aacf410ae15a2d315f3db400d`, is MIT.
See `LICENSES/llama.cpp-MIT.txt` and upstream notices within the separately fetched
source tree, including ggml, STB and other incorporated code. The source bootstrap
does not remove those notices. A binary package must retain the applicable
upstream notices alongside its corresponding source.

The pinned CPU build also compiles the following vendored dependencies. Their
actual notices are copied unchanged or extracted from the pinned source into
`LICENSES/` and installed with the application:

| Component | Source within llama.cpp | Notice |
|---|---|---|
| JSON for Modern C++ 3.12.0, Niels Lohmann | vendor/nlohmann/json.hpp | nlohmann-json-MIT.txt |
| stb_image, Sean Barrett | vendor/stb/stb_image.h | stb-image-MIT.txt (MIT alternative) |
| miniaudio, David Reid | vendor/miniaudio/miniaudio.h | miniaudio-MIT-0.txt (MIT-0 alternative) |
| xxHash, Yann Collet | vendor/hash/xxhash | xxHash-BSD-2-Clause.txt |
| SHA-1, Steve Reid | vendor/hash/sha1 | sha1-Public-Domain.txt |
| SHA-256, Igor Pavlov | vendor/hash/sha256 | sha256-Public-Domain.txt |
| rotate-bits, William Casarin | vendor/hash/rotate-bits | rotate-bits-MIT.txt |
| subprocess.h, sheredom | vendor/sheredom/subprocess.h | sheredom-Unlicense.txt |

The mtmd library includes audio/model helper code even though this application
does not expose audio search. Notices follow compiled code rather than advertised
features. GGML/llama remain covered by the pinned upstream MIT notice. This list
covers the current CPU configuration; enabling other backends requires inspecting
their additional dependencies. A source release must retain the pinned runtime
revision, source bootstrap and patch so its corresponding upstream source is
recoverable; binary release assembly remains a separate validation step.

**EmbeddingGemma 2** by Google DeepMind is a separate, optional download, not
included in this repository or application package. The pinned Google model card
declares Apache-2.0 and refers deployments to the Gemma Prohibited Use Policy:

- [Pinned model card](https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md)
- [Apache license from Google](https://ai.google.dev/gemma/apache_2)
- [Gemma Prohibited Use Policy](https://ai.google.dev/gemma/prohibited_use_policy)
- [Pinned GGUF conversion by ggml-org](https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/tree/bfcd298762cc34d0357ece5ebdd31791a3a374d8)

`LICENSES/Apache-2.0.txt` contains the Apache license text. Preserve the model card
and any upstream notices if redistributing weights. The app downloads only the
two pinned Q8_0 files after the user chooses model setup and verifies their hashes.

Qt 6 and KDE Frameworks are supplied by the operating system. Poppler's
`pdftotext`/`pdftoppm` and Tesseract are invoked as external tools. Their packages
retain their own license notices. They are not copied into this repository.

Operating-system dynamic libraries are not bundled by the current staged recipe.
Final source/binary archive contents and any future bundled-library notices remain
to be checked before release.

The retained validation evidence additionally contains upstream **reference source
snapshots** from Qt, KDE, xdg-desktop-portal and Linux. They are inspected evidence,
not linked into the application. Preserve their original copyright/SPDX headers;
`evidence-source-licenses/` records a bounded header inventory and corresponding
GNU GPL/LGPL texts, with GPL3 in the root `LICENSE`. The public export profile
omits four unnecessary copied system binaries while keeping original identity
receipts and documenting historical manifest omissions. See
[PUBLIC_SOURCE.md](docs/PUBLIC_SOURCE.md) before preparing a public repository.
