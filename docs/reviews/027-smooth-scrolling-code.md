---
story: S027
title: "Objection record: S027 smooth scrolling in the editor view"
mode: code
spec: docs/stories/027-smooth-scrolling.md
branch: feature/smooth_scrolling
commits_examined: "0ee602b..debe80e (13dd591, b056494, debe80e)"
date: 2026-11-08
reviewer: Advocatus Diaboli (Claude, the session that also makes the fixes)
objections:
  - id: O1
    category: failure
    severity: major
    title: "A scroll by the program while the animation runs is undone by the next frame"
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: operational
    severity: major
    title: "The clang-tidy job fails on the QPointer member of SmoothScroller"
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: major
    title: "The reversal test depends on a frame within 40 ms and is red on macOS"
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: design
    severity: major
    title: "Touchpad and trackpad scrolling is animated and measured in wheel notches"
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: failure
    severity: minor
    title: "applyPendingShift may run before the scroller exists"
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: operational
    severity: minor
    title: "Tests and code arrived in one commit, not test-first"
    disposition: pending
    disposition_rationale: null
  - id: O7
    category: operational
    severity: minor
    title: "The story is In review while the CI is red"
    disposition: pending
    disposition_rationale: null
  - id: O8
    category: design
    severity: minor
    title: "Forwarding the wheel from embedded tables is not in the story"
    disposition: pending
    disposition_rationale: null
  - id: O9
    category: cost
    severity: minor
    title: "The cost of a frame is not measured"
    disposition: pending
    disposition_rationale: null
  - id: O10
    category: operational
    severity: minor
    title: "The S026 test writes the toolbar colours twice"
    disposition: pending
    disposition_rationale: null
  - id: O11
    category: operational
    severity: minor
    title: "The version is raised to 0.3.0 inside a feature branch"
    disposition: pending
    disposition_rationale: null
---

# Review S027 — Smooth scrolling in the editor view (code)

Scope: the commits `13dd591`, `b056494` and `debe80e` on `feature/smooth_scrolling` (base `0ee602b`),
read against the story `docs/stories/027-smooth-scrolling.md`, CLAUDE.md, ADR 0003, S019,
`docs/theming.md` and `docs/performance.md`. The CI run on `debe80e` was red in the clang-tidy job
and in `tst_s027_smooth_scrolling` on macOS.

Build and probes in the cloud environment (Ubuntu 24.04, Qt 6.12.0 from conda-forge, preset `dev`,
offscreen). Probes were temporary and are not part of the branch:

- a temporary test slot in `tst_s027_smooth_scrolling` that sets the scroll bar while an animation
  runs;
- a spike outside the repo: a bare `QVariantAnimation` (200 ms, OutCubic), the time of its first
  frame, and its value after `QTest::qWait(40)` with the event loop blocked for 0, 30 and 60 ms.

Line numbers refer to `debe80e`.

## O1 - failure - major

**Objection.** While an animation runs, `SmoothScroller` ignores every change of the scroll bar's
value that it did not make itself (`smooth_scroller.cpp:28–32` updates the target only when no
animation runs) and writes its interpolated value on every frame (`onValueChanged`). A scroll made by
the program during the 200 ms is therefore undone by the next frame.

**Evidence.** Probe: 400 lines, one wheel notch down, then `bar->setValue(bar->maximum() / 2)`
(3431) while the animation runs; after 300 ms the value is 57, the end of the wheel animation.
Programmatic scrolls exist and grow: `EditorWidget::showPosition` (S028 on main, used by Find next
and previous; the keys go to the find bar, so `keyPressEvent` never stops the animation), the scroll
anchor in `applyTheme`, and caret moves from embedded tables and properties whose keys do not reach
`EditorWidget::keyPressEvent`.

**Why it matters.** The animation wins over an explicit jump the user asked for. After the merge
with main, a match found during a wheel animation is scrolled away again.

## O2 - operational - major

**Objection.** `run-clang-tidy-18` reports `clang-analyzer-cplusplus.NewDelete` ("Use of memory
after it is freed") in `qsharedpointer_impl.h`, reached from the implicit destructor of
`QPointer<QScrollBar>` at `smooth_scroller.cpp:56` (member at `smooth_scroller.hpp:53`). The job
treats warnings as errors and is red, so the branch cannot be merged.

**Evidence.** CI log of `debe80e`; reproduced locally with `clang-tidy-18 -p build/dev -quiet
src/ui/smooth_scroller.cpp`. The pattern is known in this project as a false positive of the
analyzer's reference counting; NOLINT helps only at the reported line and the analyzer then reports
the next path.

**Why it matters.** The project requires clang-tidy over src, tests, bench and fuzz without a
finding.

## O3 - failure - major

**Objection.** `reversingWheelDirectionRedirectsImmediately` waits a fixed 40 ms and then requires a
value above 0 (`tst_s027_smooth_scrolling.cpp:104–110`). The test thereby asserts the timing of the
animation timer, not the behaviour of the scroller.

**Evidence.** CI on macOS: `'intermediate > 0' returned FALSE`. Spike: the first frame of a
`QVariantAnimation` arrives after about 20 ms; with the event loop blocked for 30 ms after the start,
`QTest::qWait(40)` returns with no frame and the value still at the start. The other tests of the
file wait with `QTRY_VERIFY_WITH_TIMEOUT` and pass. `rapidSuccessiveWheelEventsAccumulateTarget`
has the same dependency (`QTest::qWait(30)`), only without asserting an intermediate value.

**Why it matters.** The test is red on slow runners without a defect and green on fast ones with
one.

## O4 - design - major

**Objection.** `wheelEvent` takes `angleDelta()` first and uses `pixelDelta()` only when the angle is
zero (`editor_widget.cpp:390–399`); every event is animated for 200 ms. Qt's documentation of
`QWheelEvent::pixelDelta()` says the value "should be used directly to scroll content on screen" and
that one should prefer it "on platforms where it's available". On macOS a trackpad event carries
both deltas, so the distance is computed from wheel notches (three lines per 120) instead of the
pixels the system delivers, and the system's own momentum events are animated a second time.

**Evidence.** Code at the lines above; the story's scenario "touchpad pixel deltas animate smoothly"
asks for the animation; the test sends pixel deltas with a zero angle
(`tst_s027_smooth_scrolling.cpp:204`), which real trackpad events do not have.

**Why it matters.** Trackpad scrolling, the usual way to scroll on a Mac, lags behind the fingers and
travels the wrong distance.

## O5 - failure - minor

**Objection.** The constructor connects `BlockLayout::batchFinished` to `applyPendingShift`
(`editor_widget.cpp:58`) and creates the scroller only at its end (`:95`), while
`applyPendingShift` now dereferences `verticalScroller_` unconditionally (`:155`). A batch with a
shift during construction would dereference a null pointer. The same holds for the unconditional
`stop()` calls in `resizeEvent`, `mousePressEvent` and `keyPressEvent`.

**Evidence.** Code reading; no crash observed, because the constructor sees no laid-out content.

**Why it matters.** The safety depends on the order of statements in a long constructor.

## O6 - operational - minor

**Objection.** Tests and implementation came in one commit (`13dd591`); no red test was committed
first, and there is no review record.

**Evidence.** `git show --stat 13dd591`.

**Why it matters.** CLAUDE.md requires test-first with red tests committed, and an objection pass
per story with a record under `docs/reviews/`.

## O7 - operational - minor

**Objection.** The story and the backlog say In review (`027-smooth-scrolling.md:3`,
`docs/stories/README.md`), while the CI on the branch is red.

**Evidence.** The status line; CI run of `debe80e`.

**Why it matters.** In review requires green CI on three platforms (docs/stories/README.md).

## O8 - design - minor

**Objection.** `TableView`, `CellEditor` and `TableWidget` now ignore wheel events and send them to
the editor's viewport (`table_view.cpp:37–46` and the three `wheelEvent` overrides). This changes the
behaviour over tables, but no scenario of the story describes it; only the test
`wheelOverEmbeddedTableScrollsEditor` does.

**Evidence.** Story text; `table_view.cpp`.

**Why it matters.** The story is the contract for review and acceptance; behaviour outside it is not
accepted with it.

## O9 - cost - minor

**Objection.** One wheel notch used to cause one scroll; it now causes a frame every ~16 ms for
200 ms, each with the scroll's consequences: lazy layout of newly visible blocks (ADR 0003),
`TableEmbedder::refresh` (about 0.3 ms per scroll step) and the styler's visible range. No
benchmark or measurement shows that a frame stays within budget on the 10 MB benchmark document.

**Evidence.** `bench/` has no S027 case; `docs/performance.md` names no frame budget for scrolling.

**Why it matters.** "The UI never blocks" is a project rule; a jerky animation defeats the story's
purpose.

## O10 - operational - minor

**Objection.** `debe80e` changes the S026 test `themedToolbarUsesEveryToken` to compare the bar's
pixels with `QColor(0x22, 0x24, 0x2a)` and `QColor(0x44, 0x48, 0x52)` instead of
`*theme.toolbar.background` and `*theme.toolbar.border` (`tst_s026_floating_toolbar.cpp:206–207`).
The same values are set as tokens at the top of the test, so each colour is now written twice, and
the change belongs to S026, not to this story.

**Evidence.** Diff of `debe80e`. The dereference of the optional tokens is what
`bugprone-unchecked-optional-access` reports, which is the likely reason for the change.

**Why it matters.** The link between token and expected pixel rests on two literals that have to be
changed together.

## O11 - operational - minor

**Objection.** `debe80e` raises `project(... VERSION 0.1.0)` to `0.3.0` (`CMakeLists.txt:5`). The
tag `v0.3.0-tulip` points at `b056494`, where the file still says 0.1.0.

**Evidence.** Diff of `debe80e`; `git show b056494:CMakeLists.txt`.

**Why it matters.** Versioning and automatic packages are an open topic of their own
(`docs/stories/unrefined.md`); a version number that arrives with a feature merge is not tied to a
release decision.

## Capped concerns

- A range change during `applyPendingShift` may move the target to the new maximum before `shift()`
  adds the delta again; the clamp hides it today.
- The `QEvent::Wheel` branch in `viewportEvent` repeats what `QAbstractScrollArea` already does.
- The signals `scrollStarted` and `scrollFinished` have no receiver.
- The forwarded wheel event keeps positions relative to the table, not to the editor's viewport.
- `Options::smoothScrolling` is neither stored nor offered in the UI.
- The operating system's reduced-motion setting is not considered.

## Explicitly not objecting to

- **Easing and duration.** OutCubic over 200 ms is a common choice and matches the story.
- **Accumulation and reversal.** Same-direction notches add to the target, a reversal starts from the
  current position; both are tested.
- **Stopping on direct interaction.** Slider press, scroll bar actions, mouse press, key press and
  resize stop the animation.
- **Clamping and the option.** Targets are clamped to the scroll range, and the option switches back
  to Qt's own wheel handling, including Ctrl and Shift.
