# Native development build

This is a development candidate, not a released installation recipe. The actual
development host is Arch Linux, Qt 6.12.0, KDE Frameworks 6.30.0, GCC 16.2.1,
CMake 4.4.4 and Ninja 1.13.2. CMake requests Qt >=6.5 and KF6 Runner >=6.0;
older versions have not been tested. CPU inference is the validated foundation.

Dependencies: a C++17 compiler with OpenMP, CMake, Ninja, Git, Python 3, Qt 6
Core/Gui/Widgets/QML/Quick/QuickControls2/Network/Sql, Kirigami, KF6 Runner/CoreAddons/IconThemes and
Extra CMake Modules. The Qt SQLite driver must be installed. Poppler CLI tools
(`pdftotext`, `pdftoppm`) and Tesseract with English data provide PDF/OCR support.
The program reports unavailable extractors instead of fabricating content.

On Arch these correspond to `base-devel cmake ninja git python qt6-base
qt6-declarative kirigami krunner kiconthemes extra-cmake-modules poppler tesseract
tesseract-data-eng`. Dependency installation is a separate administrator action;
the project scripts do not change system packages or configuration.

From a checkout:

```sh
./tools/runtime_setup.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
```

The source helper fetches the exact llama.cpp revision and verifies the exact
no-logits patch and dirty source state. It does not download weights or build
unrelated upstream tools. Local weights and runtime sources are ignored by Git.

Run `build/bin/kde-index`. Its default model setup is explicit in the app. Review
the linked model card/license/policy, then choose Download model. The two files
total 864,676,480 bytes and are checked by size and SHA256. There is no account,
telemetry or remote query inference. Selecting folders is a separate action.
For an existing verified development download, use
`build/bin/kde-index --models "$PWD/models"`.

For a staged installation that touches no system files:

```sh
DESTDIR="$PWD/local/stage" cmake --install build --prefix /usr
local/stage/usr/bin/kde-index --models "$PWD/models"
```

The app finds the adjacent service and embedding executable and the installed
model helper under `share/kde-index`. It passes an actual executable SHA256 to
the service, whose database persists the complete model/pipeline/runtime identity.
An incompatible identity is refused and requires deliberate index rebuilding;
existing vectors are never silently mixed with a new producer.

The KRunner module installs as `lib/qt6/plugins/kf6/krunner/kde-index-runner.so`
in this prefix. KDE derives the stable plugin ID from that filename.
A staged directory alone does not expose it to the user's Plasma session.
The corrected public source snapshot was rebuilt, installed to `/usr` in an
owned Arch userspace, queried through the actual app and default KDE plugin paths,
then removed by its exact verified manifest. See
[the current evidence](../evidence/public-install-v2/report.md). This check uses
the host Wayland connection and disposable configuration; it does not install
anything in the user's live session or claim a booted fresh Plasma desktop.
In the current adapter, use lowercase `find ` followed by the query.
Dolphin receives a native `--select` action. Okular is optional; the app can
preview the matching PDF page internally when it is unavailable.

The development runner-v5 candidate replaces `<`, `>` and `&` with readable
fullwidth glyphs in result titles, snippets and errors. This prevents the stock
KRunner tooltip from interpreting indexed text as HTML. Original file URLs,
action paths and the app's literal previews retain their original characters.
The affected privacy repair passed independent review for the tested
Qt/Plasma consumer versions. Ordinary stock result layout has separate native
Qt rendered readback evidence. The app's text Open-file branch has separately reviewed original-file consumption
through the portal in a private profile with one recommended handler. See
[the review and its limits](../evidence/desktop-open-review-v1/report.md). This does
not qualify arbitrary handlers, stock KRunner Enter, physical presentation or
release acceptance.

For direct synthetic window/service validation only, configure with
`-DKDE_INDEX_BUILD_UI_SMOKE=ON`. `tools/ui_smoke.cpp` requires an explicitly
supplied disposable corpus and deletes its `manual.pdf` as a lifecycle control;
never give it a personal folder. This is an integration runner, not a general
application command. Evidence reports distinguish build, protocol, actual CPU
integration, target-window and release acceptance.
