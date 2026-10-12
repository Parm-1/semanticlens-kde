# Runner-v5 author delivery: display privacy repair

Status: frozen author candidate, pending uninvolved evidence review and parent integration. This report does not approve the design. The parent owns root wiring, canonical GOAL updates, final fresh source build and release acceptance.

## Current trigger and scope

The actual trigger is the active first-release goal after an actual stock KRunner automatic HTTP fetch from an indexed HTML passage. Gemma/CPU/GPU questions are already answered; the validated CPU foundation stays unchanged. The author owns only linked `src/runner-v5`, new runner-privacy helpers and this package. Original runner-v4/v3/evidence and root build/docs/GOAL were not changed. No model/engine/service/GUI/ranking/cancellation changes, personal-root indexing, Baloo/Plasma configuration changes or publication occurred.

## Changed display contract

Before assigning `QueryMatch.text` and `QueryMatch.subtext`, v5 substitutes ASCII `<`, `>` and `&` with U+FF1C FULLWIDTH LESS-THAN SIGN, U+FF1E FULLWIDTH GREATER-THAN SIGN and U+FF06 FULLWIDTH AMPERSAND. All other characters retain their prior presentation, including the existing whitespace collapse and title/passage/error clipping. The replacement is deliberate and visible: `R&D` displays as `R＆D`, and literal source `&lt;` displays as `＆lt;`. The main application's literal PlainText preview stays original.

This applies at all four assignments: diagnostic title/subtext and ordinary result title/subtext. It therefore covers source-derived titles, text/PDF passages, PDF page detail, empty-image-passage path fallback and service-error detail. It does not transform the indexed data, serialized search query/reply, match ID, URLs or action data. Existing row validation, ranking, exact identifiers, reply budget and cancellation code are unchanged. `runner-v4-to-v5.diff` is the complete source change. CMake and metadata remain byte-identical to v4.

## Primary producer/consumer evidence

The desktop analyst's original stock control is frozen in `evidence/desktop-integration-v1`: all 105 pins verified, manifest SHA256 `4d5a6cc2280d92ddcf1331806aaf2a42960618b7e94fafdda6385424788caf5f`, report `bfb757719d9559d01f0141d910bf12dc0f8e73200999bc05aac3e3c3187fa756`. Its original `raw/run8/results.json` records the uninstrumented stock peer, actual indexed service reply and `/passage` GET 65,185,433ns later; this falsifier remains unchanged. Original report/pins are also copied under `original/` for explicit linkage. This package uses the copied primary source snapshots, not the author's expectations, for the mechanism:

- `primary/runnerresultsmodel-v6.30.0.cpp` forwards QueryMatch text and subtext as display/subtext model roles.
- `primary/installed-ResultDelegate.qml` assigns those fields to ordinary title/subtext PlainText labels (title is StyledText only for multiLine) and forwards their text to both tooltips. multiLine does not change the tooltip contract.
- `primary/installed-ToolTip.qml` leaves its internal Label at AutoText and strips ASCII `&` before a nonspace character. HTML escaping would therefore lose readable entity delimiters in this installed consumer.
- `primary/qtextdocument-v6.12.0.cpp` lines 73–130 shows the actual rich-text heuristic: leading XML/DOCTYPE handling, ASCII `<` recognized tags and ASCII `&lt;`. Once ASCII `<` and `&` are absent, all rich-text admission routes in this pinned implementation are excluded. Replacing `>` as well keeps balanced markup-looking text readable.

`primary/packages.txt` identifies Plasma/Milou 6.7.5, KF6 6.30.0 and Qt 6.12.0. This source reasoning is version-bounded; it is not a claim about every future consumer/parser.

## Native fixture controls

`manager-v5.jsonl` contains original serialized fixtures and actual linked KF6 RunnerManager matches using the built plugin and Qt 6.12.0. Eleven focused cases cover ordinary exact identifiers, hostile title, HTML image passage, DOCTYPE, XML, literal entities, ordinary ampersands, PDF detail, empty image path fallback, service error and clipping. The checker reports 22 inert display fields; their actual Qt `mightBeRichText` results are false before and after the installed tooltip's ampersand operation. Ten source URLs/action maps and IDs retain original paths, and relevance is unchanged. All fixture peers observed disconnect and all fixture threads joined/sockets were removed.

The same helper against the accepted v4 module is preserved in `manager-v4.jsonl`: eleven raw-rich display fields remain rich under actual Qt. Ordinary `R&D` and literal `&lt;` also demonstrate the tooltip's loss of ASCII ampersands. These are contradictory raw controls, not flags supplied by the candidate. Native fixture rows are synthetic serialized evidence; they do not establish actual service producer behavior.

## Actual stock frontend affected check

`stage-identity.json` checks all 54 files in an owned copy of the accepted clean-build-v3 prefix. Only its runner module changed. Service SHA256 remains `3176fa48374a69e35b2b6b064e68dc86baf852dfd73d49c66716c569fe575845`, probe `3dd4ddd9a133e9861ec834a2434121f99872b2ac94b1b211690a620357408414`, GUI `7362440ed0d4df35f1b8a8a58121db08a349cfe199f2849a1d22c7bd252f56d9`. Author v5 module SHA256 is `522baa458d9b2b20de311b21e2454c576573b2eba6fde512f0704beefbed25c1`. This mixed author stage is distinct from the parent's pending fresh-root final source build.

Both `native-run1/` and `native-run2/` use real `/usr/bin/krunner` on the actual Wayland session in an owned private PID/network/session namespace, an actual staged service and verified CPU model/probe, and only two manufactured owned files. The relay forwards the service bytes unchanged; it never injects result fields. The valid file name is `<b>TITLE-77331.txt`. Its actual indexed source passage contains a live local `<img src="http://127.0.0.1:PORT/passage" />`, literal `&lt;ENTITY&gt;`, `R&D` and `A & B`. The second file is intentionally an invalid PDF, not a fabricated successful index row.

Each raw `results.json` retains source hashes, successful index status, SO_PEERCRED identity, original requests/replies, exact event times and HTTP requests. Connection 0 is an uninstrumented `/usr/bin/krunner`; it receives the original title/passage/URL from the actual service. The later observer process reports the mapped staged plugin and actual instantiated title/subtext/toolTip Label objects. The title and passage remain recognizable with fullwidth delimiters, including entities/ampersands; the GUI shows the original literal PlainText fields.

Each run has two live-canary positive GETs, before stock query and after the actual current-result/hover consumption phase. Neither run records `/passage` at any time. Thus four positive HTTP requests and zero passage requests were observed across two bounded real-source runs. This directly covers the predecessor's demonstrated automatic fetch from instantiated tooltip consumers, which did not require a physically shown popup.

The affected action also passes: native keyboard navigation selects the actual source delegate and invokes its reveal action; a fresh actual DolphinView reports `file:///tmp/work/corpus` with selected item `file:///tmp/work/corpus/%3Cb%3ETITLE-77331.txt`. The fullwidth display title never becomes the action path. Original app/open/benchmark suites were not duplicated.

## Preserved viewport falsifiers and limits

Run1's stock View reports height 38 and ResultsView width 0. The passage is logically current and its AutoText tooltip Label is instantiated, but the label is not truncated and the popup stays closed. The screenshot is a query bar; node `visible`/`effective_visible` flags are not evidence of scene intersection or physical presentation.

One declared geometry successor, run2, changes only actual View.height to 160 and ResultsView.width to 550 using the existing observer. Both assignments succeeded, but label layout still reports passage width about 1545px, `truncated=false`, and closed popup. All model, text, textFormat and tooltip visibility fields remained source-owned. This control is preserved and does not qualify a visible popup, default layout or compositor presentation. No third native attempt was run.

The actual automatic-fetch route and instantiated AutoText values are qualified; physically displayed result/tooltip geometry remains a separate parent integration boundary. Fixture branches for image-path fallback and dynamic service error are native fixture evidence only. No whole-corpus privacy/performance, GPU, future Qt behavior or opened-content claim follows. Parent's portal open boundary is outside this repair.

## Outcome branches and stop rule

For the pinned consumer, the author result is: raw service HTML survives unchanged; assigned display text contains no ASCII markup/entity delimiters; actual Qt classifies those display strings as plain; the source-derived stock consumers receive them and the live HTTP witness records no passage fetch while controls remain reachable; raw actions select the original file. Independent review decides whether this supports accepting the bounded contract.

If independent review finds an untouched source-derived assignment, rich classified transformed field, stale/mismatched module, missing live positive witness or passage GET, this author result fails. Repair through a linked new version, preserve this sealed package and send the affected recheck to the same uninvolved reviewer. A missing physically shown popup is explicitly outside the qualified claim and remains pending parent integration; it must not be relabeled as passed. The two native runs are complete, the single geometry successor is exhausted, and native ownership is released. No additional polling or native attempt is justified without a new concrete discriminating probe.

## Delivery, closure and measurement scope

`candidate/` contains frozen source and built module; `helpers/` contains the exact fixture/native helpers and unchanged observer; `original/` links the inherited helper/v4 source and original discovery pins. Build logs, primary consumer snapshots, native raw receipts, source fixtures, screenshots and checker outputs are included. Index databases, models, runtime sockets and session data are excluded from this delivery. Final content pins are `SHA256SUMS`; root candidate/helper pins are `root-source-pins.sha256`.

Native outer helper elapsed times were 18.837906052998733s and 19.35621544499736s, covering namespace service/indexing/frontend/action/closure, not query latency or a benchmark. Full author package elapsed and model/token/billing usage were not exposed and remain unknown. No separate usage ledger was created.

Both outer execution handles completed exit0. Each service closed gracefully exit0, all owned app/KRunner processes have terminal exits, relay/canary threads closed, detached namespace children were terminated and the entire private namespace exited. Offline build/fixture handles are terminal and no author socket/server/thread remains. The native interval has been explicitly released to the parent. Next consumer: the parent verifies final pins, commissions an uninvolved evidence reviewer, then carries the reviewed source through final root wiring/build/simultaneous-client integration.
