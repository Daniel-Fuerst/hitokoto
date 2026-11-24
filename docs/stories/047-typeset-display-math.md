# S047 — Display formulas shown typeset

- **Status:** Draft · **Order:** 52 · **Size:** M · **Depends on:** S044, S045, S046
- **Basis:** ADR 0014, S044.

## Story

As a writer, I want a `$$` block drawn as one typeset formula, centred in the text column, while
the caret is outside it, and as its source while the caret or selection is in it.

## Decisions (proposed)

1. The block reserves the formula's height (with padding and margins) in `BlockLayout` on its
   first line and hides the others, as tables and properties do (`BlockLayout::Claimant`,
   ADR 0005); the UI paints the formula in a panel.
2. The block reveals as a whole while the caret or selection is inside it (as fences, S021).
   Clicking the formula reveals it with the caret at the start of its source.
3. Tokens: `math.display-background`, `math.display-border`, `math.display-border-width`,
   `math.display-radius`, `math.display-padding`, `math.display-margin`, `math.display-align`.

## Acceptance scenarios (outline)

- The Cauchy-Schwarz, Maxwell (`aligned`), piecewise with `pmatrix` and chemical (`\xrightarrow`)
  blocks of the sample show typeset and centred with the caret at the end of the document.
- Putting the caret on any line of a block shows the source of the whole block.
- Saving the sample without edits writes it byte for byte.
- Theming: every `math.display-*` token applies.
