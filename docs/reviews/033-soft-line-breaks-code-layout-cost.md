---
story: S033
mode: code
spec: docs/stories/033-soft-line-breaks.md
commits: cd33bd5..24acd13 (pushed), 827602e..56471f7 (cloud copy, not yet applied)
date: 2026-11-12
reviewer: Advocatus Diaboli (same session as the rework, no independent subagent; night mode, no questions asked)
objections:
  - id: O1
    category: cost
    severity: major
    title: "Two benchmark keys stay SLOW against main, so the pull request's benchmark job will be red"
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: failure
    severity: minor
    title: "A clamp nested inside another clamp consumes the flag, and the outer one counts as a reader's scroll"
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: minor
    title: "setSliceBlocksForTests fixes only the styling slices; level jobs and paragraph walks still end by time"
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: design
    severity: minor
    title: "After a resize, the reading position and the followed caret can disagree, and nothing decides which wins"
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: operational
    severity: minor
    title: "Undo and Redo from the Edit menu are not treated as typing, so their caret can be let go"
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: design
    severity: minor
    title: "The width sample for estimates is Latin text; CJK documents are estimated far too short"
    disposition: pending
    disposition_rationale: null
  - id: O7
    category: design
    severity: minor
    title: "kWalkedAtOnce = 512 is chosen to keep one test green, not from a measured need"
    disposition: pending
    disposition_rationale: null
  - id: O8
    category: failure
    severity: minor
    title: "The extent test allows 1,164 lines per key, more than twice what the code walks"
    disposition: pending
    disposition_rationale: null
  - id: O9
    category: operational
    severity: minor
    title: "The new order of the layout's signals is a contract change that no test pins"
    disposition: pending
    disposition_rationale: null
  - id: O10
    category: operational
    severity: minor
    title: "settleAnchor stops after four passes; the number has no test and no reasoning"
    disposition: pending
    disposition_rationale: null
  - id: O11
    category: cost
    severity: minor
    title: "tst_s033 takes 10 s instead of 3 s"
    disposition: pending
    disposition_rationale: null
---

# S033 — Advocatus Diaboli, mode code (rework: layout cost, reading position, caret)

Reviewed: the commits of `agents/s033-layout-cost` from the reading position on (cd33bd5 to 24acd13,
pushed) and the commits of the night of 2026-11-11 in the cloud copy (827602e, bbe8848, 020c17f,
2d2a1b4, 56471f7): `src/ui/editor_widget.cpp`/`.hpp` (reading position, settleAnchor, the caret the
view follows, clamps), `src/ui/block_layout.cpp`/`.hpp` (estimate from a sample of running text,
order of `blockHeightChanged`/`batchFinished` and `documentSizeChanged`),
`src/ui/block_styler.cpp`/`.hpp` (`setSliceBlocksForTests`, `extentSteps`, the extent search that
stops past the screen), `tests/ui/tst_s033_soft_line_breaks.cpp`, `tests/ui/tst_s019_editor_surface.cpp`
and the story's rework section.

## O1 - cost - major

**Two benchmark keys stay SLOW against main, so the pull request's benchmark job will be red.** In
the cloud benchmark (base cec1655, branch 2d2a1b4, four rounds, gate run locally):
`s023_open_10mb_styled_ms` 2,025 -> 2,602 ms (+28 %) and `s031_paste_no_table_5000_lines_ms`
24.7 -> 33.4 ms (+36 %). The branch is meant to make PR #37's benchmark pass; it does not yet. The
story lists both as "Open", but nothing decides whether they are fixed, accepted with the label
`bench-accepted`, or left to Daniel.

Evidence: gate report of the cloud run (87 of 93 keys ok); callgrind of the 10 MB opening:
`BlockLayout::draw` 15.9 G against 12.5 G instructions on main (77,879 against 69,308 blocks drawn,
about 13 % more per block), styling cheaper than on main.

## O2 - failure - minor

**A clamp nested inside another clamp consumes the flag, and the outer one counts as a reader's
scroll.** `rangeChanged` sets `rangeClamps_` when the value no longer fits; the next `valueChanged`
takes it as the clamp. But the clamp's `valueChanged` first reaches QAbstractScrollArea, whose
scroll restyles and lays out blocks on screen; a new size there can change the range again and clamp
again inside. The inner `valueChanged` reaches the editor's handler first and clears the flag; the
outer one then finds it cleared and lets go of the reading position and, with the caret off screen,
of the followed caret. A backtrace taken during the work showed exactly such a nesting
(`QAbstractSlider::setValue` from `QWidgetTextControl::documentSizeChanged` inside a scroll).

Evidence: `src/ui/editor_widget.cpp` lines 84-85 and 101; slot order of `valueChanged` (Qt's
scroll area connects first).

## O3 - failure - minor

**setSliceBlocksForTests fixes only the styling slices; level jobs and paragraph walks still end by
time.** `idleSlice` uses the fixed block count, but `levelJobs_` and `paragraphSearches_` (S023's
setext walks, which a "-" below a paragraph starts) still run for `kSliceNanoseconds` per slice.
`changingTheFlowOfALongParagraphAgainLaysOutNoMoreOfIt` and
`theCaretStaysOnScreenWhileALongParagraphIsRestyledInSlices` are therefore not fully deterministic;
they passed ten times in a row after the fix, which shows the fix, not that the slicing is fixed.

Evidence: `src/ui/block_styler.cpp` lines 699-700 against 971-978.

## O4 - design - minor

**After a resize, the reading position and the followed caret can disagree, and nothing decides
which wins.** A resize restores the top line; when the caret is followed and the narrower width
pushes it below the screen, the queued caret check scrolls to the caret and lets go of the reading
position. Both behaviours are reasonable, but the story does not say which one the reader gets, and
every resize test scrolls first, which ends the following.

Evidence: `src/ui/editor_widget.cpp` `resizeEvent`, `keepCaretOnScreen`; tests
`aResizeAndBack…`, `resizingKeepsTheReadingPosition` all scroll before the resize.

## O5 - operational - minor

**Undo and Redo from the Edit menu are not treated as typing, so their caret can be let go.**
`typing_` is set in `keyPressEvent`, `inputMethodEvent` and `insertFromMimeData` only. Undo or Redo
clicked in the menu (`QTextEdit::undo`/`redo`) scrolls to the caret from layouts the edit has not
updated yet, as a key does; if that scroll leaves the caret off screen, the following ends and the
caret can stay off screen while the restored text is restyled.

Evidence: `src/ui/editor_widget.cpp` lines 669, 687, 696; `src/ui/main_window.cpp` lines 109-110.

## O6 - design - minor

**The width sample for estimates is Latin text; CJK documents are estimated far too short.** The
sample is "The quick brown fox …, 1234567890."; a CJK character is about twice as wide. A document
in Japanese or Chinese gets estimates of about half its height until it is laid out, so the scroll
thumb and `keepContentStill` absorb large corrections. The brief names CJK fonts; the old
`averageCharWidth` had the same Latin bias, so this is not new, but the rework touched the estimate
without considering it.

Evidence: `src/ui/block_layout.cpp` lines 340-344.

## O7 - design - minor

**kWalkedAtOnce = 512 is chosen to keep one test green, not from a measured need.** Without a
minimum, tst_s002 `onlyAffectedLinesAreRestyled` restyled 22 lines instead of at most 18, because a
9-line paragraph at the edge of the screen was revealed in two steps. 512 makes that test pass, but
a paragraph of 600 lines crossing the edge of the screen is still revealed in two steps, and the
number is not related to the size of a screen.

Evidence: `src/ui/block_styler.cpp` lines 1131-1143; commit message of 2d2a1b4.

## O8 - failure - minor

**The extent test allows 1,164 lines per key, more than twice what the code walks.** The code walks
512 lines at most once past the screen; the test accepts `4 * onScreen + 1024`. A regression that
walks 1,000 lines per key (about 0.3 ms more) would stay green.

Evidence: `tests/ui/tst_s033_soft_line_breaks.cpp` line 1008; observed 512 steps per key.

## O9 - operational - minor

**The new order of the layout's signals is a contract change that no test pins.** `setHeight` now
emits `blockHeightChanged` before `documentSizeChanged`, and a batch emits `batchFinished` before
its size. Every listener (QTextEdit's scroll range, `TableEmbedder::reposition`,
`PropertiesEmbedder::reposition`, `keepContentStill`, the caret check) sees the new order; the fix
depends on it, but only indirectly through the caret and flow tests. A later change back to the old
order would show only as the random failures that started this.

Evidence: `src/ui/block_layout.cpp` lines 307 and 780; header comment of `batchFinished`.

## O10 - operational - minor

**settleAnchor stops after four passes; the number has no test and no reasoning.** After four moves
the held reading position is no longer shown again, whatever happens; no test reaches the limit, and
the comment does not say why four.

Evidence: `src/ui/editor_widget.hpp` line 184; `src/ui/editor_widget.cpp` line 326.

## O11 - cost - minor

**tst_s033 takes 10 s instead of 3 s.** The caret test opens three fixtures (one with slices of 64
blocks), the flow test three; on the macOS runner tst_s033 already took 7.3 s.

Evidence: `Totals: 28 passed … 9808ms` against 3.0 s on 24acd13 (Qt 6.12, cloud); macOS CI log of
28cb865 (7.3 s there before the night's tests).

## Explicitly not objecting to

- Holding the reading position as a QTextCursor plus a pixel offset: it follows edits and survives
  re-estimates, and the tst_s019 probe over 41 widths and four fonts has no red width.
- Measuring the width of running text instead of `averageCharWidth`: the table in 7787bdd shows
  the font's own average 20-30 % off for common fonts and the sample within a few per cent.
- Stopping the extent search past the screen: the idle continuation and `setRevealed` from
  `underlineSearchSlice` already existed for searches over 2 ms, so the reveal of off-screen lines
  arrives the same way as before.
- Following the caret only when it moved by itself or by typing, not when another cursor's edit
  carried it off screen: `layingOutTheScreenLaysOutEachBlockAboutOnce` shows the cost of following a
  program's insert.
- No new Qt module, dependency or ADR: all changes are inside the existing editor, layout and styler.
