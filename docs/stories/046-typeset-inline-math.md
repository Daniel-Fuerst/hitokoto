# S046 — Inline formulas shown typeset

- **Status:** In progress · **Order:** 51 · **Size:** L · **Depends on:** S044, S045, S010, S019
- **Basis:** ADR 0014, S044, S045. Started on Daniel's word on 2026-11-24.

## Story

As a writer, I want an inline `$…$` formula drawn typeset while the caret is off its line, and as
its source while I edit it.

## Decisions

1. **What the core says, what the UI does.** The core is unchanged: while the caret is off the
   line, S044 hides the dollars and marks the formula's source (`LineRender::math`, role `Math`).
   The UI replaces that source: its characters stay in the document (ADR 0002) but take no width,
   except the first, which reserves the typeset formula's width; the editor paints the glyphs and
   rules of `core::layoutMath` there, on the line's baseline. The source of a formula is never
   changed or copied into the document.
2. **Line height.** A formula taller than the line (a fraction, `\sum` with limits) makes its line
   taller: the reserving character takes a font size whose ascent and descent cover the formula,
   so Qt's own line layout leaves the room, and nothing else in `BlockLayout` changes.
3. **Size and look.** A formula is laid out at the text size of its line (headings included)
   times `math.font-scale`, with the bundled STIX Two Math (S045), in `math.text`. `$$…$$` on one
   line inside a paragraph is laid out in display style in place; `$$` blocks are S047's.
4. **A cache, so typing costs nothing.** Layouts are kept per formula source, pixel size and
   style in the theme's formats (at most 4,096; all are dropped when the cache is full and when
   the theme changes). Typing elsewhere lays out no formula; typing in a formula shows its source
   (S044) and lays out nothing.
5. **Errors stay readable.** A formula `core::layoutMath` cannot lay out keeps its source visible
   (the dollars stay hidden) in the new token `math.error`, with the reason as tooltip.
6. **Selection and find.** A selection or a find highlight that touches a typeset formula marks
   the whole formula with its background, painted behind the glyphs; find (S028) matches in the
   source as it does now, since the source is not hidden from it.
7. **Clicking** a typeset formula reveals its line (S010), and the caret lands at the formula's
   source.
8. **Accessibility.** The document text is the source, so screen readers read the LaTeX.

## Acceptance scenarios

**Scenario: an inline formula is typeset**
- Given the line `a $x^2$ b` with the caret on another line
- Then the source takes no width beyond the formula's, and the editor paints the typeset formula
  between `a` and `b`
- When the caret moves onto the line
- Then the source shows as in S044 and nothing is painted

**Scenario: a tall formula makes room**
- Given `$\frac{a}{b}$` in a line with the caret elsewhere
- Then the line is at least as tall as the formula, so the line above and below do not overlap it

**Scenario: a formula that cannot be typeset**
- Given `$\frac{a$`
- Then its source shows in `math.error` with the tooltip "missing }", and nothing is painted

**Scenario: typing stays within a frame**
- Given a document with the sample's inline formulas
- When I type on a line without a formula
- Then no formula is laid out again (the cache is hit)

**Scenario: clicking a formula**
- Given a typeset formula
- When I click on it
- Then the line is revealed and the caret is inside or at the start of the formula's source

**Scenario: selection over a formula**
- Given a selection across a typeset formula
- Then the formula has the selection background

**Scenario: theming**
- `math.text`, `math.font-scale` and `math.error` apply to typeset formulas

## Theme tokens

| Token | Visible property | Fallback |
|---|---|---|
| `math.error` | source of a formula that cannot be typeset | `colors.accent` |

`math.text` and `math.font-scale` (S044) now also apply to typeset formulas.

## Tests

- UI (`tst_s046_typeset_inline_math`): the source keeps its characters and only the first takes
  the formula's width, the formula is painted in `math.text`; the caret's line shows the source;
  a fraction and a sum with limits make their line taller; an invalid formula shows its source in
  `math.error` with the reason as tooltip; typing on another line lays out nothing (the cache);
  clicking a formula reveals it with the caret in its source; a find highlight marks the whole
  formula. `HITOKOTO_MATH_PREVIEW` and `HITOKOTO_MATH_SAMPLE_PREVIEW` write pictures of the editor.
- `tst_s044_math_source` checks source styling on the caret's line (off it, S046 replaces it).
- `S005_Theme` covers `math.error`.
- Benchmark `S045_LayoutMaxwellEquations` (core): the largest formula of the sample, about 7 µs on
  the development VM (release build), laid out once and then taken from the cache.

## Implementation

- `EditorFormats` cuts the source of each typeset formula after its first character
  (`applyFormula`): the rest takes the zero-width format of concealed link syntax, the first
  takes the formula's width as letter spacing and a font size whose ascent and descent cover the
  formula. `typeset()` says which formulas are typeset (inline, dollars concealed);
  `mathLayout()` takes layouts from a `MathCache` that copies of the formats share.
- `math_render.hpp`: `MathCache` and `paintMathLayout` (glyph runs per size, raw fonts kept per
  size).
- `paintFormulas` (`editor_painters.hpp`), called from `EditorWidget::paintDecorations` for blocks
  off the caret, paints each formula at the first character of its source on the line's
  baseline, with the selection and find highlights it touches behind it.
