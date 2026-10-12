# Source archive rebuild and owned /usr installation

The c394cca550d76b7754ffbbe41bc579b202b0c9ff development source archive rebuilt in
an independently extracted source/build tree in the disposable Arch userspace.
The resulting CPU producer passed the unchanged reference checks. Actual CMake
installation to that userspace's /usr produced the 21 expected files; native app
and default KDE plugin-discovery checks passed. Exact application-file removal
was checked afterward. This is a development proof, not the final release identity.

## Source and build

`archive-package-receipt.json` and `archive-SHA256SUMS` identify the assembly:
source SHA256 5878eb3503cd5accd4347aa48fed3adacfb4618dc85a035e008881c9d290dc97,
native SHA256 80cc50488e037d8210e9ef29a8de9c78f72c8e055ec69e7d8a5b906f6626b391.
The source archive preserves all 1,237 tracked Git blobs at the selected commit.
The native archive in that assembly is the prior stage-v5 runtime payload; the
archive-built installation tested here comes from the new build, not that payload.

The source was extracted with Python tarfile's data filter into the owned root's
`/archive-source-v1/kde-index-0.1.0-dev-archivecheck-source`. Its runtime setup,
Release Ninja configure, four-job build and staged `/usr` install used a new
build tree without inheriting vendor/build directories. The first fetch failed
because the disposable root had an empty resolver; `source-build.log` preserves
that failure. The linked retry used a read-only resolver bind for the explicit
upstream fetch, then completed all targets and staging (`source-build-r2.log`).
No host dependency transaction, config change or application installation occurred.

`observation.json`, `CMakeCache.txt` and `runtime-diff.patch` retain actual source,
producer and build identities. Runtime HEAD is the pinned
4d756bc72bf00a4aacf410ae15a2d315f3db400d; its sole patch has accepted SHA256
33c8c4812a4307aef3f398e5ba622edcc54ef421054edd98ca0f71906b9a2d7e.
This userspace has the previously recorded Arch dependencies; it is not a fresh
booted desktop. Compilation requests Qt 6.5/KF6 6.0 minimums but does not establish
compatibility with those older versions. Its CPU backend emits SSE4.2/F16C/FMA/
BMI2/AVX/AVX2 instructions; broader CPU and GPU operation remain unvalidated.

## Newly compiled producer

The actual archive-built probe SHA256 is
d5d7f4cfbceb27232e32d168d390fc232a45222373d12b13ae52d99561ec8a06.
`run-parity.py` invoked it against the same accepted public model identities and
unchanged embedding-v1 inputs. `parity/` retains raw outputs, errors and run
metadata; `parity-evaluation.log` is the unchanged evaluator's full result.
All eight upstream-vector comparisons exceed the original 0.999 threshold;
minimum cosine is 0.999791778842563. Both deliberately incorrect preprocessing
controls stay below the threshold. All four malformed inputs return no vector,
and recovery and text-only checks pass. No weights are in this evidence package.
This is correctness validation, not a new retrieval or timing benchmark.

## Actual installation and native consumers

`install-owned.py`, `install.log` and `install-receipt.json` record the actual
`cmake --install build --prefix /usr` inside the owned root, without DESTDIR.
All 21 destination paths were initially absent. Every installed byte matches the
new archive-built stage and CMake's install manifest. There are no upstream SDK
headers, static libraries or package configuration files in the installed payload.

`run-installed.py` and `native-command.json` record a private PID/network userspace
using actual `/usr/bin` application/service/probe binaries, its dependencies and
the host Wayland socket. HOME and XDG data/config are disposable. The original
installed-integration helper is from the source archive. A narrowly derived
`default-runner/` helper removes its addLibraryPath call and records Qt's actual
library paths; it makes no plugin-path injection. KDE discovers exactly the
expected stable ID at `/usr/lib/qt6/plugins/kf6/krunner/kde-index-runner.so`.
The runner operates off the main thread and completes its query.

`native/results.json` retains both raw actual search intervals and results.
App and RunnerManager requests overlap and both return the intended payment.txt
first for CARD-39312. The three synthetic text/PDF/image files index successfully.
Parent inspected `native/app.png`, showing the intended current query/result rows.
This is Qt rendered readback through the target Wayland socket, not physical
presentation evidence. This helper tests RunnerManager discovery, not stock
KRunner frontend layout or Enter activation. Portal open/reveal remain qualified
only by their separate desktop evidence. The helper's inherited 'staged' scope
label is retained in its raw receipt; actual command/path observations establish
that this run used /usr installation. Diagnostic timings are not benchmark claims.

The external shared service survives the app's capture/exit. Explicit shutdown
then returns zero; all recorded namespace PIDs, sockets and relay threads close.
The outer handle58022 finished zero and its private namespace closed. No desktop
window, service, worker or install command remains live from this package.

## Removal and remaining scope

After closure, `remove-owned.py` verifies the exact 21 installed hashes again and
removes only those initially absent owned-root application files. All are absent
afterward (`remove-receipt.json`). Dependency packages and host files are untouched.
This is exact-manifest removal, not a package-manager uninstall or a CMake
uninstall target. Build/stage trees remain ignored locally for reproducibility.

The complete later source commit must still be rebuilt, installed and assembled
for the final candidate; the unfinished local-root repair is not in c394. No
public upload, repository/developer identity decision or release acceptance was
made. Source/model/selected-root privacy and physical user interaction are not
proved merely by installation. Frozen earlier evidence is unchanged. Package
billing usage and total engineering elapsed time are unknown.
