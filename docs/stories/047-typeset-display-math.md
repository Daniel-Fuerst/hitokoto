# S047 — Display formulas shown typeset

- **Status:** In review since 2026-11-24 (CI green on Linux, Linux with the minimum Qt, Windows,
  macOS, ASan, clang-tidy, fuzzing and the benchmarks on pull request 78; the S009 gate extended for
  formulas) · **Order:** 52 · **Size:** M · **Depends on:** S044, S045, S046
- **Basis:** ADR 0014, S044. Started on Daniel's word on 2026-11-24.

## Story

As a writer, I want a `$$` block drawn as one typeset formula, centred in the text column, while
the caret is outside it, and as its source while the caret or selection is in it.

## Decisions

1. **Reserved, not a widget.** A closed `$$` block whose lines hold neither the caret nor the
   selection is typeset: its opening line reserves the panel's height in `BlockLayout` and its
   other lines are hidden, as tables and properties do (ADR 0005, `BlockLayout::Claimant`); the
   editor paints the panel and the formula there itself. No widget, nothing to edit in place: the
   source is edited as text. `MathBlockEmbedder` finds the blocks on and near the screen and
   claims their lines.
2. **The source of the formula** is the source of the block's lines (S044's `LineRender::math`,
   text beside the `$$` included) joined with line breaks, laid out in display style at the body
   text size times `math.font-scale`, from the cache of S046.
3. **Revealed as a whole.** While the caret or selection is on any line of the block, every line
   shows its source and the `$$` lines show their dollars, as code fences do (S021); the core's
   `renderDocument` reveals the `$$` lines of a block the same way. The caret reaches the block
   from above or below through its opening line, and a click on the panel puts it there.
4. **What is not typeset.** An unclosed block, and a block the engine cannot lay out, show their
   source (S044). A block that cannot be laid out shows its source in `math.error` with the reason
   as tooltip on its formula lines.
5. **The panel.** Centred formula; tokens `math.display-background`, `math.display-border`,
   `math.display-border-width`, `math.display-radius`, `math.display-padding` and
   `math.display-margin`. The built-in themes leave the background at the editor background, so
   the default shows the formula alone.

## Acceptance scenarios

**Scenario: a display block is typeset**
- Given the Cauchy-Schwarz block of the sample with the caret below it
- Then its three lines take the height of one panel, the closing and formula lines are hidden,
  and the formula is painted centred in the text column

**Scenario: the caret in the block shows its source**
- When the caret moves onto any line of the block
- Then every line shows its source, the `$$` lines with their dollars, and nothing is painted

**Scenario: entering from above and below**
- Given the caret on the line above (or below) a typeset block
- When I press Down (or Up)
- Then the caret is on the block's opening line and the block shows its source

**Scenario: what stays source**
- Given an unclosed `$$` block, and a block `$$` / `\frac{a` / `$$`
- Then both show their source; the second's formula line is in `math.error` with "missing }" as
  tooltip

**Scenario: the sample**
- Given `tests/data/s044-latex.md` with the caret at its end
- Then all four blocks are typeset, and saving writes the file byte for byte

**Scenario: theming**
- Every `math.display-*` token applies to the panel

## Theme tokens

| Token | Visible property | Fallback |
|---|---|---|
| `math.display-background` | background of a display formula's panel | `colors.background` |
| `math.display-border` | border of the panel | `colors.border` |
| `math.display-border-width` | border width, 0 to 8 | 0 |
| `math.display-radius` | corner radius of the panel, 0 to 32 | `layout.radius` |
| `math.display-padding` | space between the panel's edge and the formula, 0 to 64 | `layout.spacing` |
| `math.display-margin` | space above and below the panel, 0 to 64 | 3 |

Left alignment (a `math.display-align`) is left for a later story.

## In review

- **What changed:** a closed `$$` block off the caret and the selection is one typeset formula on a
  panel: its opening line reserves the height, its other lines are hidden (`MathBlockEmbedder`, a
  `BlockLayout::Claimant`), and the editor paints panel and formula (`paintDisplayFormula`); the
  caret or selection in the block reveals all its lines. The S009 extension found that Left from
  the line below a typeset block put the caret at the start of the hidden formula line; it now
  lands on the end of the closing `$$` (`EditorWidget::handleMathBlockKey`).
- **Tests added:** `S047_DisplayMath` (core), `tst_s047_typeset_display_math` (UI), the
  `math.display-*` rows of `S005_Theme`; S009: `typesetFormulasWhileWalking`,
  `caretEntersADisplayBlockOnItsLines`.
- **Next:** Daniel tries the sample's blocks in the app and marks the story Done;
  `math.display-align` waits in `unrefined.md`.

## Tests

- Core (`tests/core/test_s047_display_math.cpp`, `S047_DisplayMath`): the caret on a formula line
  or a selection into the block reveals both `$$` lines in `renderDocument`, text beside the
  dollars included.
- UI (`tst_s047_typeset_display_math`): the opening line reserves the panel's height and the other
  lines are hidden, the panel and formula are painted in their tokens; the caret in the block
  shows every line's source and the block is typeset again once it leaves; Down from above and Up
  from below enter the block; a click on the panel shows the source; unclosed and invalid blocks
  stay source, the invalid one with its reason; the sample's four blocks are typeset and it saves
  byte for byte. Each case also compares the styler with the core's reference
  (`differenceFromTheReference`, which now formats with the line's text so inline formulas count).
- `S005_Theme` covers the `math.display-*` tokens.

## Implementation

- `ElementRegistry::renderDocument` and `BlockStyler` (`codeExtentOf`, `updateExtentAfterEdit`)
  treat the `$$` lines of a display block as fences (S021).
- `MathBlockEmbedder` (`math_block_embedder.hpp`, a `BlockLayout::Claimant` without a widget):
  on a queued refresh after caret, selection, edits, scrolling, the styler and theme changes, it
  finds the closed blocks within 200 lines of the screen, lays out each block's source (cache of
  S046), and reserves the opening line's height and hides the other lines of every block off the
  caret; blocks it released or that changed go back to normal. Claimed lines are kept as
  `QTextCursor` positions, so edits elsewhere move them along.
- `paintDisplayFormula` and `paintFormulaErrorBar` (`editor_painters.hpp`), called from
  `EditorWidget::paintDisplayFormulas`; the reason of a block that failed is the tooltip over its
  lines (`EditorWidget::viewportEvent`).
