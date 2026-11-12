---
story: S033
mode: code
spec: docs/stories/033-soft-line-breaks.md
commits: 763b39c, e6f635c, d3053e7
date: 2026-11-10
reviewer: Advocatus Diaboli (same session as the rework, no independent subagent)
dispositioned: 2026-11-11 (proposed by Claude on 2026-11-10, confirmed or changed by Armin on 2026-11-11; O2 changed to fixed by Armin on 2026-11-11, on agents/s033-layout-cost)
objections:
  - id: O1
    category: failure
    severity: minor
    title: "Inside a paragraph of more than 64 source lines the top line moves by one block on a resize"
    disposition: documented
    disposition_rationale: "Confirmed by Armin. The limit is S033's kMaxLookback in BlockLayout::layoutParagraphChain, which keeps the 20,000-line paragraph within its budget; lifting it is a change of S033's layout, not of the scroll anchor. Recorded in the story's rework section as a known limit, left to the story's owner (Daniel)."
  - id: O2
    category: failure
    severity: minor
    title: "Height estimates ignore shared lines, so the scroll range is about half too large after every re-estimate"
    disposition: fixed
    disposition_rationale: "First confirmed as documented by Armin (2026-11-11). The same estimate drove the rest of s031_paste_no_table_5000_lines_ms (a paste at the end followed the caret again as the lines were laid out), so Armin decided the same day to fix it on agents/s033-layout-cost. Fixed by 0b3e7ee (red tests) and ed0b7b3: a block that goes on in the last line of the block before takes its share of a line (estimatedToFlowOn: as the styler set it once the block before is styled, before that when both blocks have text); a block that starts a line keeps one line at least. The macOS CI then showed the estimate 26 % too large: QFontMetricsF::averageCharWidth is 20-30 % wider than running text for Arial, Helvetica and their metric twins; d16c3f0 (red tests with such a font) and 7787bdd measure the width of a sample of running text instead. Tests: tst_s033 theEstimatedHeightOfAFlowingParagraphIsCloseToItsLaidOutHeight, theEstimatedHeightHoldsWithAFontWhoseAverageWidthIsOff, pastingManyShortLinesLaysOutTheScreenAtTheCaretAboutOnce, pastingManyShortLinesWithAFontWhoseAverageWidthIsOff. The known limit in the story is removed."
  - id: O3
    category: operational
    severity: minor
    title: "The guard against the nested resizeEvent has no test of its own"
    disposition: fixed
    disposition_rationale: "Proposed as documented; Armin chose a test. tst_s033 aResizeRestoresTheReadingPositionOnce counts the editor's own resize events and EditorWidget::anchorRestores() (instrumentation for tests, like BlockStyler::styledCount): one resize, one restore. With the guard removed it is red (2 restores for 1 resize, Qt 6.12). The guard stays as Daniel wrote it. A test through the scroll bar's values does not work, because a re-estimate changes the coordinates."
  - id: O4
    category: operational
    severity: minor
    title: "The new test finds its starting position by paging and skips where no page fits the font"
    disposition: documented
    disposition_rationale: "Confirmed by Armin. Placing the view directly on such a line did not reproduce the fault (the layout of the paragraph then reaches the anchor), so the test pages like tst_s019 does. On Linux in CI the precondition holds (Qt 6.8.3 and 6.12); elsewhere the test skips with a message instead of failing, and tst_s019 resizingKeepsTheReadingPosition covers the same case. Whether it runs or skips on Windows and macOS is read from the CI logs of the pull request."
  - id: O5
    category: design
    severity: minor
    title: "blockAt and blockCovering answer two similar questions; the wrong one is easy to pick"
    disposition: fixed
    disposition_rationale: "Proposed as documented; Armin chose new names. blockAt is now firstBlockOfLineAt (painting, hit tests, visibleBlockRange) and blockCovering is now blockOfTextAt (the reading position); the header says what each returns and for whom, and captureAnchor says why it takes blockOfTextAt. EditorWidget::firstVisibleBlock keeps its name from QPlainTextEdit and returns the first block of a shared line, which painting and visibleBlockRange need."
---

# S033 — Advocatus Diaboli, mode code (rework: reading position on resize)

Reviewed: Daniel's 763b39c (SmoothScroller shift from the bar's value, guard against the nested
`resizeEvent`, margins before `QTextEdit::resizeEvent`) as merged in ae9d45f, and on
`agents/s033-resize-anchor` the commits e6f635c (red test) and d3053e7 (fix and story), against
`docs/stories/033-soft-line-breaks.md` and S019 finding 2 (`docs/stories/019-editor-surface.md`). Code read:
`src/ui/editor_widget.cpp` (`captureAnchor`, `restoreAnchor`, `keepContentStill`,
`applyPendingShift`, `resizeEvent`, `applyTheme`), `src/ui/smooth_scroller.cpp`,
`src/ui/block_layout.cpp` (`blockAt`, `blockCovering`, `layoutParagraphChain`,
`ensureBlockLayout`, `estimatedHeight`, `advanceOf`), the tests `tst_s019_editor_surface`,
`tst_s027_smooth_scrolling` and `tst_s033_soft_line_breaks`.

Evidence was gathered in a clone in the cloud environment (not the user's computer; the device
bridge was closed), with Qt 6.8.3 and 6.12.0 from conda-forge, `QT_QPA_PLATFORM=offscreen`, and
temporary `qWarning` probes that were never committed.

## O1 - failure - minor

**Inside a paragraph of more than 64 source lines the top line moves by one block on a resize.**
`layoutParagraphChain` looks back at most 64 blocks for the start of a paragraph
(`kMaxLookback`), so after a re-estimate the anchor's paragraph is laid out from a block that is
not its real start, on a fresh line, and its line breaks differ from the flow above until those
blocks are laid out too. With the new test's document changed to one paragraph of 960 source lines
and the view paged deep into it, the block at the top was 892 before and 893 after the first resize
(Qt 6.8.3). Short paragraphs, the common case, are exact.

Evidence: `src/ui/block_layout.cpp`, `layoutParagraphChain` (`constexpr int kMaxLookback = 64`);
probe run of `resizingKeepsTheTopLineInsideAFlowingParagraph` with one 960-line paragraph (892 →
893).

## O2 - failure - minor

**Height estimates ignore shared lines, so the scroll range is about half too large after every
re-estimate.** `estimatedHeight` gives every visible block at least one line, but in a flowing
paragraph most short source lines share a line and their advance is 0. In tst_s019's document the
scroll range at the end was 29,555 px once laid out and 43,292 px right after a resize to the same
width. The scroll bar's thumb jumps on every resize or theme change, and `keepContentStill` has to
absorb large corrections while the reader scrolls back up.

Evidence: `src/ui/block_layout.cpp`, `estimatedHeight` and `advanceOf`; probe output of
`resizingKeepsTheReadingPosition` (max 29,555 before, 43,292 after).

Disposition (2026-11-11, Armin): fixed on `agents/s033-layout-cost` (ed0b7b3, 7787bdd); see the
frontmatter.

## O3 - operational - minor

**The guard against the nested `resizeEvent` has no test of its own.** Removing the `resizing_`
check from `EditorWidget::resizeEvent` leaves `tst_s019` and `tst_s033` green on Qt 6.8.3 and 6.12:
since the anchor comes from the line at the top and the scroller shifts from the bar's value, the
outer call restores the right position either way. The guard can be removed or broken later
without any test noticing.

Evidence: `src/ui/editor_widget.cpp`, `resizeEvent`; probe run without the guard (all green).

## O4 - operational - minor

**The new test finds its starting position by paging and skips where no page fits the font.**
`resizingKeepsTheTopLineInsideAFlowingParagraph` pages down until the top line belongs to a block
at least six blocks behind the first block of its shared line; where that never happens with the
platform's font, it skips. A version that places the view directly on such a line passed without
the fix, because laying out the anchor's paragraph then covered the blocks in between.

Evidence: `tests/ui/tst_s033_soft_line_breaks.cpp`; probe runs of the direct placement (green
without the fix on Qt 6.8.3 and 6.12).

## O5 - design - minor

**`blockAt` and `blockCovering` answer two similar questions; the wrong one is easy to pick.** The
fault came from `captureAnchor` using `firstVisibleBlock()`, which is `blockAt` and returns the
first block of a shared line, correct for painting and hit tests and wrong for a reading position.
Both functions now sit next to each other in `BlockLayout`.

Evidence: `src/ui/block_layout.hpp`; `src/ui/editor_widget.cpp`, `firstVisibleBlock`,
`visibleBlockRange`, `captureAnchor`.

## Explicitly not objecting to

- Daniel's `SmoothScroller::shift` from the bar's value without an animation: its test in
  `tst_s027` is red without it on both Qt versions, and it removes the jump back to the anchor of
  the in-between width.
- Setting the margins before `QTextEdit::resizeEvent`: Qt still lays the viewport out once with the
  old margins, but the outer call works with the final width.
- `blockAt` (now `firstBlockOfLineAt`) stepping back to the first block of a shared line for
  painting, hit tests and `visibleBlockRange`: those need every block of the line.
- No new Qt module or dependency and no ADR: the change is a query on the existing height index.
