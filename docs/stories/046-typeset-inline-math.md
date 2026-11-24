# S046 — Inline formulas shown typeset

- **Status:** Draft · **Order:** 51 · **Size:** L · **Depends on:** S044, S045, S010, S019
- **Basis:** ADR 0014, S044.

## Story

As a writer, I want an inline `$…$` formula drawn typeset while the caret is off its line, and as
its source while I edit it.

## Decisions (proposed)

1. The formula's source is concealed and its typeset width reserved in the line, baseline
   aligned; the UI paints the glyphs and rules S045 lays out, with `QRawFont` of the math font.
2. Layouts are cached by source, font and size; typing elsewhere lays out no formula, typing in a
   formula shows source and lays out nothing.
3. A formula that cannot be typeset shows its source in `math.error` with a tooltip naming the
   reason.
4. Clicking a typeset formula reveals its line and puts the caret at the start of its source
   (S010). Find (S028) matches in the source and reveals it.
5. Accessibility: the typeset formula's accessible text is its source.

## Acceptance scenarios (outline)

- The inline formulas of the sample show typeset with the caret elsewhere, as source with the
  caret on the line.
- `$\frac{a$` shows its source in `math.error` with the reason as tooltip.
- A keystroke in a formula stays within one frame; scrolling the sample holds 60 fps.
- Theming: `math.text`, `math.font-family`, `math.font-scale`, `math.error` apply.
