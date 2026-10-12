# Dependency and distribution notes

Checked 2026-10-11. These notes identify upstream sources and release work;
the component builds and CPU proof are recorded under evidence/.
The integrated desktop installation and release remain under validation.

## Embedding pipeline

- [DigUp](https://github.com/ARahim3/DigUp/tree/6faa29e4c315a3a591de1b41bd78751bdf39588d):
  inspected commit `6faa29e4c315a3a591de1b41bd78751bdf39588d`, MIT. Copied pipeline
  code and synthetic reference fixtures retain its notice in `probe/` and the
  embedding evidence package. Its references cover text, images and audio; our
  initial proof covers only the first two modalities.
- [llama.cpp](https://github.com/ggml-org/llama.cpp/tree/b11461): MIT, pinned `b11461`
  plus DigUp's no-logits patch. CPU and GPU capabilities must be validated per
  pipeline/backend; generic backend support is not model correctness evidence.
- [Google model card](https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md):
  source revision `914f7f89142e33e77833254d9c9b90c3cef7303b`; Apache-2.0 metadata.
  It also refers deployments to the Gemma Prohibited Use Policy. Preserve the
  exact notices and applicable model terms in the download/release flow.
- [GGUF conversion](https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/tree/bfcd298762cc34d0357ece5ebdd31791a3a374d8):
  revision `bfcd298762cc34d0357ece5ebdd31791a3a374d8`, Apache-2.0 metadata verified
  through Hugging Face's revision API. Text and media Q8_0 files total 864,676,480
  bytes. The download helper must verify both size and SHA-256 from the producer.
  Model files stay outside Git and are not bundled in the application by default.

## Native application and readers

The current application uses Qt 6/Kirigami and KF6 Runner/CoreAddons/IconThemes.
Source extraction uses Qt SQLite and external Poppler/Tesseract executables. The host currently provides Qt 6.12.0
and KDE Frameworks 6.30.0, CMake 4.4.4, Ninja 1.13.2 and GCC 16.2.1.
This inventory establishes development availability, not minimum version support.
The build requests Qt >=6.5 and KF6 Runner >=6.0; only the recorded current
Arch versions have been exercised. A fresh Arch userspace component build and
producer parity passed in evidence/clean-build-v1; that is not an older-version
or booted desktop compatibility result.

SQLite FTS5 supplies exact-text retrieval. Poppler CLI tools and Tesseract are
available for PDF and OCR extraction. FFmpeg is available for later media support.
Check licenses for actual linked libraries and packaged tools, including transitive
dependencies and third-party notices. Keep extraction subprocess argument handling
and error reporting explicit; no shell interpolation of user paths or text.

Original application code is GPL-3.0-or-later. Incorporated upstream work keeps
its own notices; see THIRD_PARTY_NOTICES.md and LICENSES/. Selecting an application
license does not replace upstream notices or optional model terms. Final staged
source/binary archives still need their dependency-notice inventory checked.

One reproducible native Linux installation path is required for the first release.
Flatpak can follow, but the native KRunner adapter and a sandboxed UI need a defined
host integration path rather than assumed direct plugin installation.
