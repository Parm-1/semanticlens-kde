# Search-first native redesign validation

This package validates the development redesign requested after v0.1.0-rc1.
The published release and its evidence are unchanged. The screenshots show
synthetic documents and images, using the current KDE system palette.

## Interface and target observations

The search workspace now has a prominent search field, file-type filters,
thumbnail grid and compact list views. A persistent side panel shows the
selected image, matching PDF page or selectable document passage, with Open
and Show in Dolphin actions. Narrow windows switch to a full-width preview.
Model setup, folder management and exclusions are in Settings. Indexing issues
remain visible in the main workspace and open their detailed failure list.

The current Wayland run in `results.json` passed all 37 checks with zero QML
warnings and zero loopback source-markup fetches. It used the actual service,
verified local model and CPU producer, and a disposable synthetic root. It
checked matching PDF page rendering, loaded thumbnails and image previews,
grid/list switching, filters, keyboard selection, preview hiding and reopening,
Settings, literal source text, stale selection clearing and actual PDF extraction
failure reporting. The window was tested at 1180×780 and 720×520. The narrow
preview check confirms containment within the window, not merely a large width.
`indexing-error.png` shows the real failure in both status and Details.

`pdf-lifecycle-results.json` retains the existing focused PDF test: all ten
checks passed, including captured paper and ink pixels, preview-file removal,
immediate stale clearing and automatic refresh after deleting the owned test
PDF. Its paths refer only to the supplied synthetic corpus. Its Qt readback
records `window_exposed=false`; this supports rendered-window behavior, not
physical-user presentation. No personal folders or Baloo configuration were
used or changed.

The staged application, loading its compiled QML rather than the test's source
QML file, separately rendered `staged-app.png` and exited successfully with an
empty log. The stage has 22 runtime and notice files. `identity.json` pins the
tested source and every staged file. The CPU producer was reused byte-for-byte
from the previously validated release build; this is not a fresh inference
build or a new retrieval benchmark. Search core, service, ranking, model
pipeline and KRunner adapter source are unchanged.

## Media boundary and affected review

Thumbnails and previews use asynchronous, bounded local snapshot rendering.
Only current results inside selected roots and exclusions are admitted. Cached
output is cleared on invalidation, and source-derived text remains literal.
The media helper uses two serial lanes and descriptor-bound process-group
cancellation. An independent review found an identity race in the initial
numerical process-group cleanup; the repaired candidate uses transferred group
pidfds and an acknowledgement before rendering starts.

The [linked independent review](../redesign-media-review-v2/report.md) resolved
that finding on the exact current media source. Its native group, lifecycle and
Backend controls passed at their stated scopes; the predecessor review remains
available. Media requires Linux 6.9+ group-scope pidfd support. The unsupported
branch was checked through a process-local syscall injection on the current
kernel, not by booting an older kernel. Text results remain available when that
media capability is unavailable. See [build requirements](../../docs/BUILD.md).

Earlier failed observer, layout and rendering attempts are preserved locally;
this package records the final integrated run. The narrow layout feedback loop
and the PNG scaled-reader timeout were reproduced and repaired before this run.
Representative authored PNG/JPEG fixtures were rendered; this does not establish
general photographic quality or exhaustive decoder compatibility.

All parent build, UI, service and helper handles completed. The independent
review records its own descendant and temporary-file closure. This validation
does not install into the live Plasma session, merge a pull request or publish
a successor release. Final delivery is a feature branch and local development
candidate, with hosted build results reported separately.
