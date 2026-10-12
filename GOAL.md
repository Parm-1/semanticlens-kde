# KDE Index: project goal

Working name: KDE Index. This is an independent project; KDE affiliation and a final public name are not established.
Created: 2026-10-11. Status: active. Owner: Codex in the originating KDE Index thread.
Canonical workspace: this repository. This file owns scope, acceptance and continuation state.

## Outcome

Ship a public, open-source, native KDE Plasma 6 application that lets people find
their own files by describing what they contain. Add meaning and visual search
alongside fast exact-text search, and return the relevant passage, PDF page or
image. All indexing and inference run locally on explicitly selected folders.
Work alongside Baloo and expose results through KRunner.

The project is intended for other Linux users, with a reproducible build,
documented installation, sensible resource use and evidence-backed feature claims.
The active goal covers the first public release candidate and publication after
the user confirms the repository identity and public publication.

## First release

- Native Qt 6/Kirigami app: folder selection, search, result previews and filters,
  indexing progress, pause/resume, exclusions and index removal.
- Text/Markdown, PDFs with extractable text, JPEG and PNG. Scanned PDFs and screenshot
  text use OCR where supported; report extraction failures explicitly.
- Hybrid semantic and exact-text ranking. Preserve useful exact identifiers.
- Results identify their source passage/page/image; open files and reveal them in
  Dolphin. Use an in-app page preview if external page navigation is unavailable.
- KRunner adapter with cancellation and stale-query handling; expensive inference
  stays outside the Plasma/KRunner process.
- Incremental indexing, deletion/rename reconciliation, restart recovery and
  explicit removal of excluded content from the active searchable index.
- CPU operation as the correctness baseline. GPU support is advertised only for
  backends actually tested with this model and pipeline.
- Reproducible source build, one validated Linux installation path, CI, dependency
  and model notices, release instructions and an honest limitations section.

Later expansion: audio/video search returning timestamps, opt-in code search,
additional formats and distributions. Preserve segment and worker interfaces for
these features; they are not requirements for the first release.

## Initial architecture and invariants

Use Qt 6/C++ with a Kirigami/QML frontend, a separate llama.cpp embedding worker,
and SQLite FTS5 plus vector storage. Connect the app and KRunner adapter to a
shared search service over a local interface. Keep model loading and extraction
outside UI threads. Validate the IPC contract before implementing multiple clients.

Reuse DigUp's proven ideas and eligible source selectively. Do not attempt to
compile its Apple-dependent engine unchanged. Reference inspected upstream commit:
https://github.com/ARahim3/DigUp/tree/6faa29e4c315a3a591de1b41bd78751bdf39588d
Retain required attribution for copied code; verify licenses for the complete
distribution, including model files, before selecting the release license.

Index only selected local roots. Do not crawl all of home by default, follow links
outside the selection, alter source files, fetch cloud placeholders, or enable code
indexing implicitly. Exclude hidden directories, credentials, caches and generated
data by default. Model downloads are explicit and hash-verified; indexed content
and queries never go to a remote service. No account or telemetry is required.

Every vector is bound to model, runtime/pipeline and preprocessing identities.
Do not mix incompatible vectors after an update. Failed extraction or embedding
is an observable failure, never a successful row with fabricated content.
Baloo's configuration and database remain owned by KDE; this app uses its own index.

## Acceptance evidence

1. Linux inference proof: reproduce upstream reference inputs with pinned model
   files and the relevant upstream vector tolerances. Check text and image vectors
   and actual retrieval on the intended machine. A successful build alone fails
   this criterion. Classify CPU and each GPU backend separately.
2. Retrieval proof: freeze a small redistributable corpus, query list, relevance
   labels and ranking criteria before tuning. Include paraphrases, untagged images,
   exact identifiers, confusing near-matches and queries with no relevant file.
   Compare against Baloo with content indexing enabled for supported formats.
   Initial release target: at least 80% top-five recall for labeled semantic/image
   queries and at least a 15 percentage-point gain over Baloo on that subset;
   every deliberately unique exact identifier must retrieve its intended file.
   Report per-modality results and false positives, not just an aggregate score.
3. Lifecycle proof: directly exercise edits, renames, deletions, exclusion changes,
   interrupted indexing, app restart and simultaneous app/KRunner requests.
   Removed or excluded content must cease appearing after reconciliation.
4. Target proof: validate the installed candidate on the user's Plasma 6/Wayland
   desktop and a clean documented install environment. Record hardware, dataset,
   warm/cold query latency, indexing throughput, RSS and index size. Initial
   responsiveness target is warm-query p95 <= 1 second on the frozen corpus;
   typing and cancellation must remain responsive during indexing.
   Heavy workers exit when unused, and idle operation performs no repeated inference.
5. Publication proof: clean checkout builds using the documented process; CI checks
   the meaningful supported paths; demo data is redistributable; private files,
   indexes and machine-local configuration are absent from the release.
   Review consequential ranking, lifetime, privacy and compatibility claims with
   an independent reviewer. Publish only claims supported by actual target evidence.

These are initial targets, not achieved measurements. Record necessary contract
changes and their reasons before rerunning a revised evaluation; never silently
change labels or criteria to make a candidate pass.

## Ownership and action boundary

Codex owns implementation, focused validation, repairs and delivery. The user
owns material product choices and final public repository/name/publication approval.
Routine reversible local work, scoped commits and preparation of a reviewable
release candidate proceed autonomously. No paid service is required by this design.
Subpackages may be delegated under the user's supplied routing instructions;
one writer owns each shared surface, and independent reviewers do not author the
candidate they review. Do not manufacture management infrastructure for this goal.

## Current checkpoint and continuation

Actual trigger: automatic continuation of the active implementation goal.
Answered requests: feasibility assessment, comparison with default KDE search,
and creation/assignment of the public-release goal.
Standing direction: own the implementation and validation with minimal supervision.

Completed: scope/acceptance design, local Git initialization, dependency inventory,
and an isolated Baloo comparison-method control. `evidence/baloo-baseline-v1/`
records two content-indexed synthetic files and seven query controls passing.
The readonly-host-root sandbox, daemon and session bus closed after the run.

The CPU embedding probe and public model download/build helpers now exist in
`probe/` and `tools/embedding*`. Frozen author delivery `evidence/embedding-v1/`
contains five text and three image references, all passing upstream raw-dot
agreement >= 0.999 (minimum 0.99979171), exact resize fixtures, negative controls,
error/recovery responses and diagnostic retrieval. Parent verified all 52 evidence
pins and the 12 root source/helper pins, and independently recomputed the raw dots.
Independent review in `evidence/embedding-review-v1/report.md` reproduced the
references and found no blocking defect in that narrow CPU proof. The owner has
accepted the CPU foundation; production callers must verify actual model and
executable identities and persist them with the index.

The service author delivered frozen `evidence/engine-v2/` after 74 current
integration checks and 32 affected repair controls. Full-page literal matching,
separate FTS tier, extractor-version identity, accepted-control cancellation,
failure readiness, fatal-worker closure and explicit wipe/rebuild are implemented.
A prior .9605s status-lock observation is retained; separate snapshot locking
reduced final diagnostic status maximum to4.4ms. Same uninvolved `engine_review`
found no blocking defect in its affected v2 recheck; parent verified all24 candidate
and12 review pins and accepts these bounded repairs. Report SHA256 is
b3b4c868f7b342aa650ecdebe4d898d8e4e565e16b8bad126ce2a3c8c16465f4.
A linked portable delivery in evidence/engine-review-v2-portable contains all
30 byte-identical reports/scripts/raw receipts, with all31 pins verified. Manifest
SHA256 fa064259c0bd674df3e9a9e7c2db492eefe6817c6e11b06a3473b085730cb68e.
Frozen originals are unchanged. No corpus-wide responsiveness/recall claim.

Parent froze `evidence/ui-v1/` with native/staged window checks and actual installed
producer parity (eight raw-dot references, minimum0.99979171). Independent review
then found automatic HTML image fetch, exited-service relaunch, connected-unready
recovery and closed-phase harness defects. The same author delivered linked
`evidence/ui-v2/`: all77 pins and10 canonical source/helper matches verified.
Manifest75948a6d2027c9176328e29d07c5ca438ed710942e6fe0cf6a775b0a85abb43e;
report070fa5fa596f263cf42f673b118cac282b3337142d9757f9abcca5f5b66f6b6e.
Plain source rendering also avoids discarded styled tooltip/default dialog title
AutoText surfaces. Bounded PID/startticks/peer-credential ownership, replacement,
explicit retry/wipe and actual closed-phase cleanup are implemented. Author
controls pass, all11 known PIDs closed, no sockets/preview files. No UI acceptance.
Same uninvolved ui_review is continuing its interrupted affected recheck; passed
surface observations remain, missing final report is not a new commission.
All predecessor bytes stay unchanged. Parent now owns integration; any review repair uses a linked successor.

KRunner v1/v2 evidence is preserved. V2's 1100ms response budget failed cold
queries. Parent accepts the affected v3 repair after uninvolved review in
`evidence/runner-review-v3/report.md`: two single-paste queries after confirmed
producer unload return in 1725/1891ms, warm samples 43/57/56ms, and reset suppresses
stale matches. Its 8000ms budget and <=30ms context polling remain responsive.
The roughly ten-second idle observation spans unloading; it is not ten seconds
of continuous absence. Actual installed discovery later found filename-derived ID libkde-index-runner,
despite the explicit metadata-constructor controls passing the intended ID.
A source/native analyst established the two constructor contracts and delivered
src/runner-v4: PREFIX empty and raw Id removed, runner.cpp byte-identical to v3.
Parent verified67 portable pins and6 root-source pins, accepts this ordinary
packaging fix after4 metadata/6 real implicit native query controls, and now
wires v4. Manifest101affd44bac1c88a1840d3fe3071655e694a762bb6eac436420dc589402590c.
No live global KRunner configuration was touched.
Installed discovery/actions and integration with repaired engine remain pending.

`benchmark/` now freezes 28 authored CC0 text/PDF/chart/screenshot files and 37
queries with labels and scoring criteria, before retrieval. Its inventory SHA256 is
`2fdb07dba3b3191a7073b7fa1443e58c542fa3c03e3275d429e54058c8996872`.
Frozen author result `evidence/retrieval-v1/` has12 verified pins, manifest
94b1bc5e635d64e7cfbe19257d8d055378c105775d8a85f3d291a0406b4e6ec8.
All20 semantic queries hit top5 (17 first), Baloo0/20; all6 identifiers and8 literal
controls hit first. Four semantic PDFs return labeled pages. Warm p95 is42.05547ms
on185 repetitions; cold verified-reload samples1.957–2.750s. Indexing28files/38vectors
21.8448s, sampled summed RSS1127164KiB, apparent index1412976bytes. Baloo28 content-done
files,8/8 literals/5/6 identifiers, warm CLI p95 22.7241ms. All3 no-relevant queries
return5 irrelevant KDE Index suggestions; no cutoff. This is an authored semantic
corpus, not population/photo-quality proof. Independent retrieval_review independently
recomputed the source/page/ranking totals and found no blocking defect at the
stated authored-corpus scope. Parent verified all66 review pins and accepts this
narrow retrieval/42ms warm-p95 evidence. Review report SHA256
d08b912b0b97904d78143994fc57e003aaeb7eedfa57200cfd8994d5a301623d.
Resource figures remain reported sampled tree peaks/transient pre-shutdown index
sizes, not independently reconstructible sample traces or whole-GUI memory;
service indexing excludes initial model startup, Baloo includes daemon startup. All native
benchmark workers/sockets/sandbox closed. No integrated clean install test exists.
No GitHub remote or public publication exists yet.

Next action: consume affected GUI-v2 review and linked v3 render-phase repair,
then validate installed desktop discovery/actions and simultaneous clients.
Finish full clean successor validation, notices/CI/release preparation. Finish source/binary notices/CI and prepare public identity and
publication approval. No personal root or frozen benchmark label may be changed.

Live work: engine review and portable delivery are closed;
UI-v2 review closed with two render/accessibility findings. UI author delivered
linkedv3 (63pins verified, manifest8ad359a82dad54b9a8f89cfc93925bb37d91fbe977ce04ce2b49561a192ff207):
Ready/current-source/geometry followed by retained Qt readback and page pixels,
plus headerItem accessibility. Original blank negatives fail; Backend byte-identical.
Same uninvolved ui_review owns focusedv3 recheck. UI author and runner discovery
analyst have no live resources; parent owns source/integration again;
retrieval review is closed; parent owns integration,
benchmark tooling and a separate owned Arch bootstrap build root. Its official
2026.10.01 tarball SHA256 was verified against archlinux.org; dependency setup completed
inside the isolated owned root, with no host package transaction. Its initial
pacman download-user switches could not work in the single-UID namespace; only
the owned root configuration was adjusted, and the subsequent transaction exited0.
Some system-user/tmpfiles hooks warn because this build namespace maps only one
UID; package installation returned0 and its complete version inventory is saved.
This is a fresh userspace build root, not a booted KDE system. The full integrated source/install
build still awaits a frozen GUI successor.
CI draft exists but has not run remotely. Runtime fetch completed. The clean Git
checkout at a8eef7b compiled the CPU producer, service and reviewed runner.
The fresh producer also passed all eight unchanged upstream raw-dot references
(minimum0.99979171), negatives and recovery. Frozen evidence/clean-build-v1 has18
pins verified, manifest SHA256 d3f6c8d82477548b82809d04b059a5d0fc76d7a206904a4f3569427de9b745c7.
That commit contains complete development sources but its old UI is explicitly
blocked by review; no install claim follows from component compilation.
Full fresh source build/staging at dd18a81 passed; installed producer
3dd4ddd9a133e9861ec834a2434121f99872b2ac94b1b211690a620357408414
also passed unchanged upstream parity. evidence/clean-build-v2 has28 pins
verified, manifest20dc53d56ccbe4b36d543d63c4a204f4d4f986fb2b52e9f54b9f2c299164cad8.
Fresh userspace generic-style native Wayland controls show all10 flags and normal
app query, but actual page screenshot is blank: harness observes raster/URL before
asynchronous Image/frame rendering. Dialog Accessible.name also warns about Popup
attachment. These are retained falsifiers, not accepted preview-pixel proof. Same
UI author owns linked v3 investigation/repair; parent and reviewer preserve v2.
Parent host installed run-v1 ended: UI query/app exit0/common service preserved,
helper exit4 due filenameID mismatch, overlap false, all knownPIDs/sockets/relay
threads closed. tools/installed_integration.py records exact proxy wire intervals;
parent next repeats with corrected staged module then validates native actions.
No parent benchmark/build/parity/native/GPU-inventory handle remains. GPU questions
answered: real Gemma2 inference is currently CPU; host RTX2060 6GiB/driver615.78.08
is visible throughVulkan; shaderc/glslc present, no CUDAcompiler observed. No GPU
embedding correctness/speed claim or production GPU mode is enabled.
Resume ui_review; parent next stages corrected runner-v4/GUI-v3 in the clean root.
No baseline Baloo subprocess remains active.

## Latest integration checkpoint — 2026-10-12

Actual trigger: continuation of the active goal, with completed Gemma/CPU/GPU
questions as steering. Gemma2 is the validated CPU baseline; GPU inventory alone
does not enable a GPU backend. Earlier design/goal requests remain answered.

Clean userspace source checkout cda875b built and staged GUI-v3/runner-v4. Parent
observed build handle4291/native handle19776 terminal exit0; no parent native
process remains. Frozen evidence/clean-build-v3 has33 verified pins, manifest
bbbeb8c75505bd1fd4824bd82870a592a876244f6514cd56b7971ccb674ec88e.
Installed producer/service are byte-identical to accepted clean-build-v2. All10
GUI controls pass; genuine rendered PDF page pixels match the raster at MAE0.2703
and ink correlation0.8182. This is Qt readback with window_exposedfalse, not physical
compositor presentation. Old blank captures remain immutable. UI reviewer completed
the source-v3 affected recheck without blocking defect and is consuming these
installed receipts before final sealed delivery. No UI acceptance is inferred yet.

Desktop integration analyst owns the next private native KRunner/Dolphin interval;
parent will not start colliding native windows. Stock tooltip AutoText source is a
hypothesis requiring actual indexed passage/hover/live-canary observation, not yet
a demonstrated leak. Real actions, source selection and simultaneous clients remain
pending. No live user Plasma/Baloo configuration is to be altered.

Parent updated README and docs/BENCHMARK.md with independently reviewed narrow
retrieval results and false positives. Next: consume GUI review and desktop actual
frontend/actions evidence, repair demonstrated defects through linked successors,
finish installation/archive/CI/release validation, then obtain final public identity
and publication confirmation. Goal remains active; no GitHub remote/publication.

GUI acceptance: parent verified all67 independent UI-v3 review pins and report
690ce296f7e2bd9e710827befa89bfbedf77428fba3ac88efb10afcc7d3bc172
(manifestc1a77a2e7633c7dba861cfa818ec84089c4e7b8e406db2063c5ce69f07d1eaa3).
Accept affected GUI-v3 render/accessibility within Qt readback scope, together with
unchanged previously reviewed backend/privacy/ownership. Reviewer closed resources.

Desktop analyst reports a real indexed-passage network fetch in uninstrumented
stock KRunner: service raw HTML source reaches Milou's PlainText subtext plus the
Plasma ToolTip Label's AutoText; live positive HTTP control precedes automatic GET.
Its final frozen report is pending, not yet parent-accepted evidence. Ordinary
title/subtext labels do not contain this tooltip boundary. Runner-v4 is blocked
for release by this production privacy finding; UI acceptance does not cover it.

New bounded Sol/high runner_privacy_repair owns NEW src/runner-v5 and own helpers/
evidence only, settling actual Qt/tooltip semantics before a minimal adapter
display repair. Raw paths/actions, CPU producer, service, ranking and UI stay
unchanged. Parent owns root integration/docs; all frozen predecessors immutable.
Desktop analyst still owns the private native interval for source-faithful
frontend/action observations and corrected Dolphin selection checks. Author may
build/offline-probe but must not launch native windows before explicit release.
Next consumer is parent, followed by uninvolved affected privacy review. No new
public repository, publication or GPU inference has occurred.

Checked desktop handoff: parent verified all105 frozen desktop-integration-v1
pins (manifest4d5a6cc2280d92ddcf1331806aaf2a42960618b7e94fafdda6385424788caf5f)
and inspected raw real reply/canary65.185ms delta, actual Dolphin URL/selection and
closure. Accept the demonstrated original privacy defect and native app/runner
reveal dispatch to fresh Dolphin within that scope. Original search intervals do
not overlap. Actual file opening stops at accepted Qt/portal OpenFile FD/inode; no
response/handler within8s. Stock scene capture shows query bar only: Viewheight38,
source row outside viewport. Neither hidden instantiated labels nor model/action
invocation proves ordinary result presentation. These remain required unfinished
open/layout/overlap boundaries, not waived acceptance criteria.

Desktop author is finished, native handle74886 terminal0/allownedresourcesclosed.
Runner-v5 author completed two bounded real frontend privacy rechecks: actual
raw indexedHTML replies, live HTTP positives before/after, transformed inert
AutoText displayfields and original Dolphin action paths; no automaticpassageGET.
A geometry-controlled diagnostic still leaves tooltippopuphidden; no thirdrun
or visiblepopup/physicaldisplay claim. Bothnativehandles are terminal/allowned
resourcesclosed; author is freezing new runner-privacy-v1 before ready. Required
uninvolved affected review is next. Parent next wires reviewedcandidate, rebuilds
cleanstage and executes original simultaneous-client helper; sourcefaithful stock
layout/open discriminator remains pending after privacy review.

Runner-v5 frozen author handoff checked: parent verified all92pins and9root
source/helperpins. Manifestbeb9ff6adefd79cbbf7d112059886df2f694b6d07570731e0967b0ad2642fe01;
report30fe6912d1ac5e071668c153bdd04a623b65485534794965f7295da8e1995315.
Authorfinished/resourcesclosed (native64830/69454terminal0). NEWuninvolved
runner_privacy_review owns affectedreview and nextnativeinterval. Parent wires
frozenv5 forsourcebuild beforeacceptance; rootCMake/docs areparentowned.
Same desktopanalyst has bounded NEW desktop-boundary-v2 continuation for source/
matchedbuiltin stocklayout and portal-open discrimination; no productionwrites
or nativewindows untilreviewerexplicitrelease. Root+reviewer+analyst=3slots,
no descendants. Parentnextfullfresh-userspace successorstaging, thenserial
simultaneousclienthelper; CPUparity reusedonlyifactualproducerbytesunchanged.

Fresh userspace build/staging at8123e07 completed handle54593exit0; no buildhandle
remains. Frozen evidence/clean-build-v4 has7verifiedpins, manifest
f3eb5096ac07124e36dbd2f89d53cc21aba48980ef8fec5fce4941711cb945e8.
All54installedfiles compared: onlymodulechanged to a626737f2e6988437cc2c15d25bffcd76cdfc780588557033ef4efc8dc3fb32d;
producer/service/GUI exactbyteidentities unchanged, priorparity/GUIchecks reused.
Parent copied newstage to ownedlocal/installed-host-v3/stage/usr, but no new
native/simultaneousrun started while desktopanalyst owns nativeinterval.

Uninvolved privacyreviewer's directactualToolTip consumer control independently
reproduces originalHTTPGET evenwhenhidden; frozencandidatefieldinert and all
reviewnativePIDs/socket/handlesclosed. Finalsealedreviewpending, no parentprivacy
acceptanceyet. Reviewer explicitlyreleasednativeinterval todesktopanalyst.
Desktop-boundary-v2 owns nextboundednative matchedlayout/openprobe. Its original
Sol/highanalyst dispatchwasverifiedfromactual2026-10-12T05:39:28.428Z toolrecord,
notinferredfromcatalogpresence. Parent willnotstartcollidingnativewindow until
explicitrelease. No public/GPU/modelconfigurationchange.

Privacy acceptance: parent verified24frozen independentreviewpins, manifest
59c14e0e193ef3cd1a0b89f4101792bdf437808917dd81be37cf0c9790a63334/report
2c606a245f9535f3c9ccacf2e195c997a33d1605128159285f3da26026eba279,
inspectedactualcandidate-first0GET vsoriginalrawHTML1GET ininstalledhidden
AutoTextToolTip consumer withvalidPNG. Acceptsource-v5displayboundary for
currentQt6.12/Plasma6.7.5 consumer, unchangedrawpaths/actionmaps/protocol.
This narrowacceptance doesnotqualifyactualvisiblepopup/defaultrowlayout/open
consumer/physicalpresentation/overlap. Reviewerfinishedallresourcesclosed.

Currenttrigger remainsauthorizedgoalcontinuation; Gemma/CPU/GPUquestions and
initialdesign/assignment alreadyanswered. Currentlivechilddesktop_integration
is Sol/highanalyst newdesktop-boundary-v2/source+boundedmatchednativeprobe,
ownssoleWaylandinterval. Primaryportal1.22.1 source showsfreshprivatepermission
store/defaultassociation maystillrequirechooser; originalmonitoromittedbackend
AppChooserbranch. ActualstockrootimplicitHeight/contentHeight/ScrollView/exposure
probe isnextdiscriminator, noforcedgeometrypass. Parentstagedinstalled-host-v3
ready fororiginalsimultaneoushelperafterchildexplicitrelease. No parentbuild or
UIreview/privacyreview/nativehandle remains. Goalactive/notreleasecomplete.

## Continuation checkpoint — 2026-10-12 (simultaneous clients)

Actualtrigger: automaticcontinuation ofactivegoal; priorhumanGemma/CPU/GPU and
design/assignment questions answered. Previousgoalturnmadeprogress (accepted
privacyrepair, cleanbuild, durablefrozenreview), notno-progress.

Parent ran originalinstalledintegrationhelper against actualfreshstage-v4.
Handle44170 terminalexit0. Frozen evidence/installed-integration-v2 has17verified
pins, manifestec393488d0fcfa5ba42a9e455027137624ed05d593fc32c21a668d340d5c133f.
Actualapp ui-2 request andnativeimplicitRunnerManager requestID1 intervals
overlap1207974503ns; bothsamequery/firstpayment.txt. Off-mainrunner/discoveryID
correct, all54payloadfilesexactcleanstage-v4 identity. Externalcommonservice
survivesappcapture/exit, subsequentexplicitshutdownexit0; allrecordedPIDs/sockets/
relaythreadsclosed. Acceptnarrowactualsimultaneousoutstandingclientproof; no new
benchmark/parallelinference/stockphysicaldisplayclaim. Parentinspectedappreadback.

Desktop-v2oneboundedmatchedpair handle6431 terminalexit0/allownedresourcesclosed,
reportfreezing. Bothactualquerycompletedpositive; calculatorfirstreadbackpolishes
initialzero-widthlist/querybarlayout into properviewport. Subsequentpluginrow is
insideordinaryviewportwithoutresizes. Qtreadbackscopeonly/pluginexposedfalse.
PortalactualOpenFile→freshprivatePermissionStoremiss→KDEAppChooser choices[]/
last_choice matchesownhandler. NoDisplay=true andnomimerecommendationcache are
setupfacts; no single-factorcausalclaim. Handlercompletionstillnotqualified.

Parentcommissioned settledLuna release_packaging: NEWpackagingassembler/docs/
evidence only, exactsource4d7d167/native54filepayload, deterministictarreadback/
notices/privacyexclusions locally. No source/model/globalinstall/publication changes.
Desktopanalystremains writerforv2freeze; aftercheckedhandoff nextboundedopenpositive
canregister legitimate OWNrecommendedtext/plainhandler+normalMIMEcache using
existingKonsolecat readonlysourceconsumer, actualportal/default policy branch and
observedcontent—notfakeResponse/serial/approval. No parentnativehandle remains.
Next: consumev2pins, runexplicitboundedopen successor ifsourcecontractvalid, finish
releasearchivevalidation/finalidentity/CI/publicationapproval. Fullgoalunchanged.

Checked desktop-v2 handoff: parentverified74pins (manifest
cbb726ef22a0c3e1a521afa9bce720ad097b1dbad7b10d177a93c617b707585c/report
8811a55a22c1c4cc4b2aef4f383d5a09d6096a717e0528bcd3bfcc347fe743c1)
andinspectedactualpluginrow capture. Accept ordinarynativeQtreadbacklayout
afterdocumentedpolish/currentqueryphase, noforcedresize; physicalpresentation
notclaimed. Openhandlercompletionremainspendingnormalprivate recommendedhandler
positive; allchildresourcesclosed.

Parentchecked initialreleasepackagev1 eightROOTrelativepins/archivechecksums;
deterministicassemblymatches1134sourceobjects/54stagefiles and13notices.
Sourcearchiveoldcommit4d7d167 isdevproofonly. Actualstagealsoincludes33upstream
SDKheaders/staticlibs/CMake/pkgconfigfiles, unneededfornativeappinstallation.
RootrepairusesdocumentedCMake EXCLUDE_FROM_ALL directoryinstallsemantics plus
explicitALL CPUworker dependency: omitupstreamSDKinstallwhilebuildingworker.
Probe/pipelineCPP/CMake areunchanged; existingfrozenproofs preserved. RootCMake
andCI parentowned; newstage/output/evidencewilllinkthisinstall-onlysuccessor.
Packaginghelper/docs/reportv1readybytes remainunchanged; finalsnapshotwillinclude
assembler andnew21filepayloadafteractualcleanstagingverification.

## Continuation checkpoint — 2026-10-12 (archive installation and local-root boundary)

Actual trigger: active-goal continuation. The Gemma, CPU/GPU and initial goal
questions are already answered. The previous turn and this turn made progress;
no blocked audit applies. Nothing has been published.

The install-only successor at c394cca has 21 runtime/notice files. All retained
bytes match stage-v4; 33 upstream SDK files were removed. Parent verified the six
clean-build-v5 pins and the ten ROOT-relative release-package-v2 pins. The latter
is a deterministic development assembly, not a final RC; its source commit does
not yet include the new v2 assembler or the current unfinished repairs.

The c394 source archive was extracted into a separate tree in the owned Arch
userspace and rebuilt without inheriting a build tree or vendored runtime. The
first runtime fetch failed because that userspace's resolver was empty; the
linked retry used a read-only resolver bind and succeeded (handle77417 exit0).
Its newly compiled producer d5d7f4cfbceb27232e32d168d390fc232a45222373d12b13ae52d99561ec8a06
passed all eight unchanged upstream references, two deliberately incorrect
controls, four malformed inputs, recovery and text-only checks (handle30281
exit0). This is producer correctness evidence, not a new performance benchmark.

Actual cmake installation to /usr in that disposable userspace succeeded:
21 initially absent destination files exactly match the archive-built stage and
CMake install manifest (handle36387 exit0). Parent's next bounded native check
uses those actual /usr binaries and default Qt plugin discovery, private data
and the host Wayland socket; handle58022 is running. No host installation or
personal config is changed. Archive-install evidence is not yet sealed.

Desktop owner reports the linked Open-file check succeeded (handle26087 exit0):
real original URI and matching portal FD, Response0, fresh Konsole displaying the
source body via native accessibility, with all resources closed. Its next action
is freezing desktop-open-v3; parent must verify pins and independent review before
accepting the new consumer claim. Parent now owns the native interval.

The local-root audit found a real remaining scope gap: mounted remote/provider
storage was not gated before reads, and directory/watch traversal had no bound.
Sol/high local_root_contract owns NEW core-v3 and new local-root evidence only.
Parent approved strict known-local storage admission, component-held descriptors,
owned content snapshots, approved local nested segments, and streaming caps:
100,000 entries, 10,000 directories, 10,000 total watch paths and 16 MiB path
storage, alongside the existing eligible-file ceiling. Incomplete enumeration
must report failure before stale-row reconciliation. Own private FUSE negative
and own private local bind-mount positive are authorized; no host mounts or
provider data. This candidate still requires independent review and integration.
No universal cloud-residency or same-user-remount sandbox claim is permitted.

Archive/install proof is now sealed: evidence/archive-install-v1 has34 verified
pins, manifest fc5fa1c84067d24c46b90f67550979455f09c2d9c0e1829e976a62c85a79cac0.
Actual /usr app and RunnerManager queries both return the intended source; KDE
discovers the stable runner ID from the standard /usr Qt plugin directory with
no added library path. The actual installed service survives app exit, then
shuts down; all known PIDs/sockets/threads close. Parent inspected the app's
rendered rows. Handle58022 is terminal0. Exact hashes were rechecked before
removing only the21 initially absent owned-root application files; all are
absent afterward. No host install/uninstall, physical presentation, default
handler or package-manager removal claim follows. Native interval released.
The collection handle96161 is no longer present; the complete observation and
all34 payload hashes were inspected before sealing, and no process remains.

Desktop-open-v3 frozen handoff verified: all183 portable pins, manifest
a85f50e9f8af44ae4756384b3cf971b94ad77fa4316a66ac93b75b7e3ea8f8e0;
report bfc65577a25b39dad828aeff44643dab54b7c4e5ccfd09699adeef4f5424be83.
NEW uninvolved desktop_open_review owns the consequential consumer/readiness
assessment in new review evidence only; parent acceptance is pending. Same
local_root_contract owner was resumed after the interruption, no old live
handle was assumed. Its uninstrumented watcher control passes both candidate
and accepted service; the full unchanged integration comparison is its next
discriminator. Local-root production wiring remains unchanged and unaccepted.
Current live capacity: parent + local-root author + desktop evidence reviewer.
No parent process/window/build handle remains. Continue the accepted repair/
review loops; the goal is active and the public candidate is still unfinished.

Open-file bounded acceptance: parent verified four independent review pins,
manifest da844672d8ed4c2977dad7fc62632a14976809d78f0bea9eeef234be51ff82ad;
report 7b75083ff38a5260bcd3c1b392339af47ecef1ce91accece81fb4cf7003234bb.
Parent inspected the actual fresh Konsole capture and native loaded screen record.
Accept the staged app's page0 text Open-file signal through real Qt/portal into
its configured private single-recommended-handler receiver. Independent review
checked full normalized content, source-B and initial blank-screen negatives,
original FD/process identities and repaired source readiness. API/Response/process
creation alone never qualifies consumption. The author helper's aggregate pass
flag is weaker than the full criteria; reuse requires the raw checks preserved
by this review. No arbitrary handler, user-default editor, stock KRunner Enter,
PDF navigation or physical input/presentation claim follows. Reviewer finished,
no review resources or native interval remains.

Local-root author reports the unchanged full integration passes74 checks for
accepted service and candidate; the earlier43-check automatic-create timeout is
retained. Its original receipt has no watch-at-create observation. Source shows
500ms watch installation with no post-install reconciliation. The same owner is
running one source-faithful discriminator: actual root inotify watch absent at
creation then present, no automatic row, explicit scan body-positive. A witnessed
gap would require a parent-owned service successor repair, not a retroactive
claim that it explains the earlier failure. Core-v3 remains private/unwired;
provider and cap controls, review, UI status display and final build are pending.

The source-faithful watch discriminator witnessed event loss before native watch
installation and qualified the missing file with explicit scan; all control
resources closed. Parent commissioned settled Luna service_watch_repair for NEW
src/service-v2/main.cpp and new direct control evidence only, compiled against
unchanged accepted core2 to isolate this repair. Its contract schedules the
existing coalesced reconciliation after any successful new watch installation,
without endless scans when every addition fails. Parent owns eventual wiring.
Local-root author retains core-v3, its namespace31/resource36 controls and seal.
An uninvolved combined core/service review follows checked author handoffs.

Parent prepared NEW src/ui-v4, copying original UI unchanged except notice and
warning display. It reads scan_error, scan_warnings, scan_skipped_providers and
watch_error through the actual status object; source-derived details remain
Text.PlainText. Existing extraction failures are retained. The new notice explains
incomplete indexing/last indexed contents, skipped folders or incomplete automatic
updates. This source is not wired into the application yet. Private Qt/QML build
completed23 targets (configure9887/build53973 terminal0). Runtime validation with
the actual repaired service is pending; compilation does not prove displayed
notice state. Original UI/open/preview/privacy proof bytes remain unchanged.
Current live authors: local_root_contract and service_watch_repair. No parent
process/window/build handle remains. Next parent action consumes their frozen
sources/receipts, wires the combined candidate plus UI-v4 and validates actual
statuses, then commissions uninvolved review before final packaging/publication.

## Continuation checkpoint — 2026-10-12 (combined candidate)

Actual trigger: active-goal continuation. All earlier human Gemma/CPU/GPU/design
requests are answered. Previous turn made progress: accepted file-open review,
archive installation and UI preparation. Goal remains active, nothing published.

Parent verified the core-v3 frozen handoff:136 pins, manifest
8fab439f9bdab541094ff38b53e9068cb557dbdffb2df4b25f95a147927c5ea7;
report edb190ab63f3daa713ba1cb78ba6ce0feab6692d7604aa586383cd238853c44b.
Root source/header match frozen copies (9b25de8e.../7a836591...). The precursor's
29 pins also verify. Exact-final namespace36/name-resource7/permission-restart4
controls preserve concrete positives and contradictory raw observations. Reused
caps36/full74 receipts retain their earlier binary scope; no blanket exact-final
claim. All author resources are closed. This is author completion for integration
and review, not privacy/lifetime acceptance.

Parent verified service-watch-v1's six ROOT-relative pins and separate report hash:
manifest5f4bc00e78cabb477aa286fead265107e4fc1e1ed9c527282e11f2b25b93c8b4;
report73754a6716d74184e46e714817989e8fc54fe6e75498ca0d1e27d833fcc3cd0c.
The source-v2 is only the five-line post-watch-install scheduling change. Matched
actual-inotify missing/new-body controls qualify its bounded repair; idle revision
stable and workers/socket closed. Broader interaction acceptance is pending.

Core additionally rejects NUL, non-round-tripping Unicode roots and invalid-byte
readdir names before lookup; a real valid U+FFFD sibling retains its own body.
Parent's actual Qt URL decoder control demonstrates %FF and %00 information loss.
UI-v4 now guards raw decoded URL path bytes before IPC, retaining valid Unicode,
percent/hash escapes and encoded separators. Notice covers scan_skipped_names,
provider skips and the actual warning list. Current UI rebuild29355 terminal0;
real status/selection window validation remains pending.

Parent wired CMake to core-v3/service-v2/UI-v4, preserving original source versions.
Integrated configure60828 terminal0; build33562 running, CPU probe disabled only
for this focused integration build. Accepted3dd producer will be supplied to its
owned bin directory; this is not the final source/package build. Next: actual
combined statuses/guard/notice target checks, frozen candidate and uninvolved
combined review, then final clean build/installation/release assembly.

The factual publication inventory found four copied system binary/library files
under desktop-open-v3/candidate/native-bin. Internal proofs remain immutable;
those copies are unnecessary for building this app and will be excluded from the
public snapshot. The existing full-tree development archive is not the public
publication candidate. Prepare a linked export/receipt explaining original
manifest omissions and preserving original raw measurements and hashes. No legal
compliance claim follows from the inventory, and no repository identity/public
publication is approved. The inventory owner finished with no live resources.

## Continuation checkpoint — 2026-10-12 (affected review repairs)

Actual trigger: active-goal continuation; earlier human Gemma/CPU/GPU/design
requests are answered. Previous turn and current turn made progress. Nothing
published, goal remains active; no completion or blocked audit passes.

Combined build33562 completed0. Independent original review-v1 is sealed with
30 verified relative pins, manifest95d738ec9d696cf55391a6ed6d84060850cf7b24bc4314713d897521156a18c7.
It found blocking F1: set_exclusions deletes unrelated cached pages when source
resolution fails EACCES, despite the preceding incomplete scan preserving them.
The same core author owns linked NEW core-v4/repair-v3, preserving uncertain rows
while deleting definite exclusions. Author build68885 completed; affected real
EACCES/EMFILE and definite-exclusion controls are in progress. No acceptance yet.
Original source and negative evidence remain unchanged.

Parent authored linked service-v3 for F2, comparing desired/installed coverage
on every watch sync, including removal-only updates. Six frozen pins in
service-status-v1 verify (manifest04d9733178ef7126e37e3b8e0439872788ee968528670336e1c478aa5566c479).
Real failed batch/11-second idle/removing-last-root replay passes all3 observations;
source-v2 successful-new-watch reconciliation remains unchanged. Build16734 and
control3920 terminal0; service/workers/socket closed. Independent review pending.

Native production UI-v4/combined-v6 check73969 completed0. Real unchanged
10,000-directory cap reports incomplete scan while SQLite and actual query retain
the genuine old passage. Parent inspected the rendered notice and intended row.
Six frozen combined-ui-v1 pins verify, manifest02ea7d3363008880a2a26f20d05df23c33401f571f1cf25df2eb2ad5c28392f9.
App0, external service survives app exit, explicit shutdown0, known workers/socket
absent. Physical input/presentation, detail-dialog navigation and native bad-URL
selection are not claimed. No timing or broad quality claim from this check.

Same uninvolved combined_storage_review reactivated for linked review-v2: visible
UI receipt/F2 now, then final core4/service3 build and F1 affected interactions
once author delivery is checked. One writer per core/service/review; parent owns
integration/acceptance and the released native interval. No parent live handle
remains. Next: checked core4 handoff, wire/build combined repairs, review, then
public-source export excluding four unneeded system copies, final clean build/
install/release audit and concrete identity/publication decision.

Core-v4 author READY checked:29 relative pins verify, manifest
6ce956e3fe5a5d64b3eb6c86526234e68501e331520ff40ba352ddbc386616b0/report
f778ea7ddacd5bcc78acf386737142463ff0d15e17736223c199bbfd71402b98.
Root cpp/header match frozen df6d56d2.../a93b79ed...; only definite exclusion
classifier/call changes. Author8 lifecycle predicates and22 focused permission/
descriptor-exhaustion predicates pass. Actual occupied0–14/soft15 intervention
qualifies descriptor uncertainty; no captured individual open errno is claimed.
All author resources closed. Parent wires core4/service3 and builds integrated-v7
before same uninvolved reviewer affected recheck; author passes are not acceptance.

## Continuation checkpoint — 2026-10-12 (public export and diagnostic repair)

Actual trigger: active-goal continuation; earlier human questions already answered.
This turn made concrete progress: original review findings repaired, visible native
notice qualified, public export rebuilt and actual exported producer checked.
Goal active, no publication or completion claim. No blocked audit applies.

Combined-v7 configure62712/build55769 completed59targets. Source core4/service3
at8b2ef4c then ca53d3ca (only export/docs added), service562efba70bff1f52770a9ac26d6f2289cd6a56946cad030be2eb44aaa6664472.
Same uninvolved reviewer independently reports F1 lifecycle8 and permission/
descriptor23 observations pass; F2 actual failed-watch/11s idle/last-root clearing
all3 pass. UI notice6-pin receipt accepted within its stated native pixel scope.
Review-v2 is NOT sealed: F3 actual descriptor-preflight failure reaches explicit
scan/exclusion replies but durable status scan_error remains empty. The new negative
is preserved in review-v2/raw/uncertainty-status/results.json,
SHA6044290a372d4eed8f8ce244d42f0582100c1d2e279ddb44ed3878513df38ff4.
Core author activated for linked NEW core-v5/repair-v4; moves only preflight into
the existing guarded scan/metric failure path. Author build21224 live at last
message; affected status/cache/watch/recovery controls next. Core4/earlier evidence
immutable. Same uninvolved reviewer awaits checked final source/binary before seal.

Public-source-export-v1 five verified pins, manifest
2e8a019e8698a28fdeabf7763f8976ca0e791b86130d02dee2ace5f3562ba065.
Two normalized ca53 source archives match SHA0682c3effba8e1b2d07a7ac9259d5bb4b64b5d88cb108cae448cd616a1e0037d,
1754 retained files/37,302,555 bytes; exactly four system copies omitted with
identities preserved. Every retained Git blob/mode and tar member read back.
Original complete manifests reference omitted files and remain unchanged;
public snapshot is explicitly a subset, not a complete verified internal packet.
Internal history must not be pushed as the public repository. No publication.

Settled worker public_clean_build completed exact exported-source185-action CPU
build without inherited vendor/build. Nine ROOT-relative pins verify. The actual
21-file stage agrees with CMake's install manifest and excludes upstream SDK.
Evidence/public-clean-build-v1 preserves commands/tool versions/identities and
closure; sourceca53 is an intermediate proof, not the repaired final candidate.
Only its own new bootstrap subtree writable, system root/resolver read-only;
no /usr installation or native desktop test by this package. Worker finished.

Parent's new export-built producer45181fa5c03d19c561bc6fd37cbd00e9493f08ab751fcd75844206797447165a
passed all8 original raw-dot comparisons (mincosine0.999791778842563), two negative
preprocessing controls, four malformed inputs, recovery and text-only request.
Twenty-one evidence/public-producer-parity-v1 relative pins verify, manifest
2ec9ea83bb7619f7597e6e296f8808cdae13c124a697ca79ee36b6ab7bcf4865.
Actual new binary, network-isolated owned userspace, existing read-only models.
Initial evaluator setup lacked Python invocation; raw producer runs already0,
linked explicit-Python evaluator passed. Both failed setup and passing logs
retained; no RSS/new benchmark claim. Handle62651 terminal1 at setup assertion,
linked evaluator terminal0; namespace/producer children closed. No live parent
handle/window remains.

Source reference snapshots have preserved GPL/LGPL SPDX headers. Parent added
source-only GNU license texts and bounded68-header inventory under
evidence-source-licenses, with URL/hash receipts; not installed runtime material.
These notice files are newer than ca53 export/build proof. Final source/export/
installation must use complete eventual candidate; no blanket license compliance
claim follows from header inventory. Same core author+reviewer live capacity;
next: checked F3 handoff, combine/recheck, then final clean export/build/stage/
installed desktop and release/publication decision. Full original goal unchanged.

F3 core-v5 checked author READY:19 relative pins verify, manifest
af911acd1d6fbf11267fffda0bff29168b1b6cbb6739372910b40358ac7804bd/report
5ea71710b0cad336b630655c0f5ed27d9bf024988cb841b4dba0755487dac145.
Root cpp7c6ed298.../headera93b79ed... match frozen source; sole difference is
moving verifyExtractors into existing scan try/catch. Author permission21/
descriptor20 predicates cover truthful durable error, stopped watches/progress,
F1 preservation/removal and positive recovery; individual open errno remains
unobserved in the descriptor case. All author resources closed. Parent now wires
core5/service3 and prepares NEW integrated-v8, preserving referenced earlier
binaries. Configure27240 currently running; build and same reviewer affected
handoff next. No independent acceptance of F3 yet.

Integrated-v8 configure27240 completed0. An initial premature build returned
missing CMakeCache while configuration was still live; no source issue. Linked
build54771 completed59targets0, service1853dde0c8acee17137baeedc804bd80485d982a627745bf46411732c390a4ca,
UI261e3bce44cebe911f6296a066a28be9e1a1d1f5fac1b1e061f6338d45f13eae.
Both earlier build logs preserved. Same uninvolved reviewer received exact final
runtime and checked core5 delivery for F3/durable failure/recovery interactions;
review-v2 remains unsealed at last observation. Original F1/F2 negatives retained.

Parent actual native smoke88673 completed0 against core5/service3/UI4 and accepted
3dd producer. Ten established window/service checks pass, including actual folder
selection through guard, three-format indexing, exact PDF/page2, loaded raster/
scene ink, preview cleanup and automatic copied-PDF deletion. Parent inspected
rendered page. Fourteen combined-native-v1 pins verify, manifest
c4cbb92b6041824f856980ae249ba3fb8967845b04b4de20ae3f549c51a8cebb.
Runner0, two sampled known PIDs and socket absent; later exact executable inventory
finds no candidate service/probe. Short-lived descendants can escape sampling,
so this receipt makes no exhaustive tree claim. Native interval released. No
parent live process/tool handle. Next: consume final affected independent review,
prepare final full public export/clean build/stage/installed-target evidence and
release audit; keep original goal and publication decision intact.

Linked public-source-export-v2 three relative pins verify, manifest
e8c7a36a29140246df45bcc0e23a8b620066e6af6d733a707ebab3cadfa50f16.
Two identical normalized source5beacaebb65427478e7e2dd820c8cbea64cf13ea
archives SHA6faeb1ed7df43834eef73702265ae7a298c8e9c53b2f5c5b0b6fe865cbfcd928
contain1837 retained files including core5 and reference-source license texts.
Same four exact foreign binaries omitted; historical manifests remain unchanged.
Exporter66118/22306 terminal0. Same settled public_clean_build owner reactivated
for NEW public-clean-v2 bootstrap subtree/evidence only, oldv1 untouched. Its
next result is actual full CPU clean build and runtime-only stage from this export;
not public/review acceptance. Same independent core reviewer remains live for
F3 recheck/report-v2. No parent process/window/toolhandle remains. Next continuation
checks these exact live owners/process handles from current state, accepts only
sealed pins, then carries final installation/packaging/audit through publication
approval. Prior human questions remain answered; nothing published.

## Continuation checkpoint — 2026-10-12 (corrected installation)

Actual trigger: active-goal continuation. Prior Gemma/CPU/GPU/design questions
are answered. Previous/current turns made progress; no blocked audit. Goal active,
no publication. Parent verified all27 sealed combined-review-v2 pins, manifest
4172906df6a97ed3841a6fa6ebc1f3cb066d14447d98d28ec7f5dccc1c7f26aa/report
da87dd035bad2fe4937b93f4db67658d62d11461f16661c9e999b6b8ccf32f0c.
Accept the bounded core5/service3 F1/F2/F3 storage/status repairs: actual original
cache retained under permission/resource uncertainty, matching exclusions removed,
durable preflight failure/status/watch/recovery interactions. Broader original
provider/budget/ranking evidence retains its earlier scope. Reviewer all closed.

Eleven ROOT-relative public-clean-build-v2 pins verify; source5beaca/1837files,
full185-action cleanCPU build and exact21-file runtime stage. Candidate-source
hashes equal independently reviewed core5/service3/UI4/runner5. Exact newproducer
fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0
passed8 originalrawdotrefs (min0.9997917117943279), two negative preprocessing
controls, four malformed/no-vector inputs, recovery/text-only and realCPUinfo.
All24 public-producer-parity-v2 pins verify, manifest
1409ae5d5d7956f9b45c557fe293ce106b15830b6dd5e6af558daefa0f1eb9c4.
Its namespace/processes closed; no RSS/GPU/new release-quality claim.

Parent actual /usr installation in disposable Arch root succeeded21 exact files;
original source/build frozen with install manifest redirected to NEW own receipt.
Native check40042 completed0: installed GUI/service/probe, standard Qt /usr plugin
discovery stableID, intended sourcefirst in bothapp/RunnerManager, overlapobserved,
service survivesapp exit then shutdown0/allknownnamespacePIDs/socket/relaythreads
absent. Parent inspected rendered intended passage. Exact initially absent21files
hashedbefore removal and allabsent afterward; host/dependencies untouched. Packet
public-install-v2 is being sealed. No booted freshDE/physical/default-handler claim.

New settled release_retrieval_v2 owner runs unchanged37query/28file frozen corpus
on exact publicstage28c0service/fa7producer, comparing immutable original Baloo
raw with explicit reused-baseline scope. Parent limits concurrentwork to light
receipt/docs; no background builds/compression duringtiming. No label/tuningchange.
Next: checked current retrieval receipt, runtime/source assembly+documentedRC,
publication audit then concrete repoidentity/publication decision. Native interval
released/no parentliveprocess. Originalfullgoal remains intact.

## Continuation checkpoint — 2026-10-12 (release assembly readiness)

Actual trigger activegoalcontinuation. Earlier human questions answered, goal
active/no publication. Current turn accepted checked final storage review, exact
CPU parity and actual corrected /usr installation; all their processes closed.
Public-install-v2 is sealed17 relative pins, manifest
02d252b25a90d648621ba23b2f31ed643741aa3deeb1965bf66ae665df986afa.
Actual initialproducerpresence then namespacePID64only beforeclients, query
intervaloverlap/firstintendedsource/app/service/plugin/shutdown/removal verified.

Current exported-stage retrieval-v2 six relative pins verify; same37queries/
28files/criteria, service28c0/probefa7. Same20/20 semantic top5(17first), page4/4,
IDs6/literal8first, no-match15falsepositives. Accept narrowcurrentcorpus claims
following independent retrieval-review-v2 three verifiedpins, manifest
f6c9f4e655c7968596c69c8a94e64d5f7884fe2debee5b0e01769524f3aa3315/report
0e015f37c20f392d5caab53273f4d7abc9d15840f2877d0df2a09753acae0677.
Reviewer recomputed185rows/currentDBfield/formulacorroboration, warm47.236093ms
and225endpoint pairs. Baloo baseline is explicitly reused v1, differentclocks.
ReportedRSS/transientindexlacktrace/per-fileinventory; closedDB237568 differs
fromtransient1412976. Namespaceinnerclosureverified; outertoolcompletionreport
isnotreconstructiblefromsixartifacts. No causal/generalized/newFTS-only/GPUclaim.
All reviewers/producer/retrieval/modeljobsclosed; no concurrentmeasurementremains.

Bounded publication-payload-audit-v1 reportpin checked at ROOT, SHA
ea3b589c332a4e58dcaf297bf990e2b150524dd3752bc8cc5b450040e6318e40.
No concretecredential/userprofile identified in reviewedpaths; retainedportalconf
ispackage-defaultsnapshot. docs/ASSET_PROVENANCE preservesbenchmarkCC0, synthetic
upstreamMITfixtureprovenance and standardGPL/LGPL alternatives ratherthanundefined
customLicenseRefs. No completeprivacy/licenseauditclaim or newCC0relicensing.

Appstream offlinecheck firstwarnedmissinghomepage; prepared metainfo proposes
https://github.com/Parm-1/kde-index, contributors andOARS1.1. GitHubreadonlylogin
confirmsconnected Parm-1; repoidentity/publicationstilluserdecision. Metadata now
validates offline. NEWstage-v3 viaCmake/sourcebinding changesonlymetainfo; twenty
otherruntime/noticebytesmatchtestedstage2. Metadata-stage-v1 tenpinsverified,
manifest022441ad6789d13bad245635a6be8e4144083b76be0e9aa08069bc9d31c6e8b8.
Originalmanifestread-only; failedDESTDIRinterpretation retained, purelinkedchecker
usesactual/usrprefix. No fakefreshprocessresult/hostinstall. CI nowchecksmetainfo.

Rootpreparedassemble_release_v3: strictpublic4-fileomissionprofile+exactnative
allowlist/notice/sourceandmetainfo mapping; codeinputsunchanged since5beaca,
metadata-onlychanges explicitlylisted. Next fullcommitsource/twodeterministic
assemblies/readback, documentedRCandcompletionaudit, then finalpublicrepoidentity/
publicationconfirmation. No liveparenttool/process/window/childhandle remains.
Originalgoal and finalpublication requirement unchanged.

Final exactruntime lifecycle release-lifecycle-v1 two ROOT-relative pins verified;
all74 unchangedactualworkerchecks pass on28c0service/fa7producer/6773CLI. Current
edits/rename/delete/exclusions/interruption/cancel/pause/restart/crash/idle/wipe/
wrongidentity/privacy/OCR/status phases covered, allnamespaceworkers/services/
socketclosed. Initialmissingfixture setup retained; no candidatecodechange or
criteriareduction. Parent accepts established focusedintegration scope.

Initial RCassemblies28955/30377 terminal0; identicalsource/native hashes,
1979publicsourcefiles/21nativefiles/13notices. Source39a1bf6, codebuild5beaca;
allcompiledinputsunchanged, metadatachangesexplicitlylisted. A linked finalsource
willadd the last lifecycle/audit receipts before publication decision; initial
archivesremainimmutable. docs/RC_AUDIT mapsoriginalrequirements toactualevidence
andlimits. HostedCI/publication are stillunfulfilled; no complete/blocked status.
No liveowner/child/tool/process/window remains. Next: assemblecheckedfinalsource,
preservefinalpins, presentconcreteRCandrequestproposedParm-1/kde-indexpublicidentity/
publication confirmation peroriginalgoal. Humanquestionsalreadyanswered.


## Public source and README checkpoint — 2026-10-12

Actual trigger: automatic goal continuation following the human approval to
create public Parm-1/kde-index and publish v0.1.0-rc1, plus requests to fix the
default Git helper, rewrite the README for public users, mention Gemma, and
choose a more specific name. Earlier feasibility/design/Gemma/CPU/GPU requests
remain answered. Publication approval persists; the later name preference is
pending selection, not approval inferred from elapsed time.

The public repository exists with main at initial reviewed commit
4e0b0532478680a9bf10882a80ee98377df83c80. Hosted CI run 38310540856 completed
successfully: frozen inputs/metadata, pinned runtime, full CPU build and runtime
installation stage. The default Git wrapper was repaired to use system Git and
its matching helpers; HTTPS access passed. GitHub CLI authentication completed
with workflow access. All prior push/authentication/watch handles are terminal.

The README has been rewritten for public users with EmbeddingGemma 2, features,
local privacy, build commands, release status and bounded benchmark claims.
Current candidate/audit documents now distinguish published source/green CI
from unpublished release assets. These are documentation-only changes; sealed
evidence and RC archives stay immutable. Publish this documentation update
under the currently approved identity, then apply the selected name and qualify
affected branding before release publication and asset readback. No live test
process, desktop resource or child assignment remains. The full goal stays active.
