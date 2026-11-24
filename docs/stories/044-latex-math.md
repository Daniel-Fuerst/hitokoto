# S044 — Math source: `$…$` and `$$…$$` recognised and styled

- **Status:** In progress · **Order:** 49 · **Size:** M · **Depends on:** S014, S015, S020, S021
- **Basis:** User request on 2026-11-24 (Daniel): hitokoto should support mathematical notation,
  better LaTeX notation, with the sample `latex.md` (kept as `tests/data/s044-latex.md`). Not in
  the brief; an extension by this story and its follow-ups (CLAUDE.md, "scope changes"). On the
  same day Daniel chose an engine of our own for typesetting (ADR 0014), so the feature is split:
  this story finds formulas and shows their source styled; S045 typesets in the core, S046 shows
  inline formulas typeset, S047 display formulas. Branch `feature/latex-notation` (several closely
  related stories, CLAUDE.md).

## Story

As a writer, I want hitokoto to know which parts of my note are LaTeX formulas, written between
`$` (inline) and `$$` (display) as Obsidian, Pandoc and GitHub read them, so that their source is
set apart from the text and Markdown inside a formula is left alone, before formulas are typeset.

## Decisions

1. **Syntax as Obsidian, Pandoc (`tex_math_dollars`) and GitHub read it.** Inline math is `$…$`
   on one line: the opening `$` is not followed by whitespace, the closing `$` is not preceded by
   whitespace and not followed by a digit, and the content is not empty. A `$` that cannot close
   is part of the formula and the next one is tried (Pandoc); an opening `$` without a closer
   stays text, and so does an escaped `\$`, which neither opens nor closes. `$$` on one
   line (`$$x^2$$`) is display math written inline. `\$` is a literal dollar (S020 already
   conceals the backslash, since `$` is ASCII punctuation). In `$5.00 and another costs $10.00`
   the second `$` follows a blank, so nothing is math.
2. **Display blocks.** A line that is `$$` alone (blanks around it allowed, up to three spaces of
   indentation as for fences) opens a block that ends at the next such line. Text after the
   opening `$$` on the same line (`$$ \begin{aligned}`) belongs to the formula; a closing line
   may hold text before `$$` (`\end{aligned} $$`). An unclosed block runs to the end of the
   document, like an unclosed fence (S014).
3. **Precedence.** Fenced code blocks and inline code win: `` `$x$` `` is code, and math in a code
   block is code. Math content is opaque to the inline elements after it (emphasis, escapes):
   `$a_1 * b_2$` is no emphasis and `$\{x\}$` keeps its backslashes. Links come before math in the
   registry, but math inside a link text is still found.
4. **What shows until S046 and S047.** The formula's source is styled as math (`math.text`,
   `math.font-family`, `math.font-scale`); the `$` and `$$` delimiters are concealed while the
   caret is off their line and shown in `math.marker` while it is on it. A display block's lines
   are styled as math as a whole; revealing the whole block at once comes with S047.
5. **The core says only where formulas are.** `LineRender` gains the roles `Math` (formula
   source) and `MathMarker` (a revealed delimiter) and a list `math` of the formulas on the line
   (source span, display or inline), which S046 and S047 use to place typeset formulas. A display
   block reports its lines with `mathBlock` and its delimiter lines with `mathDelimiter`.
6. **Saving** writes the text byte for byte (S004); find (S028) searches the source as it is.

## Acceptance scenarios

**Scenario: inline math is styled and its dollars concealed**
- Given the line `Greek: $\alpha_1, \beta_2$.` with the caret on another line
- Then `\alpha_1, \beta_2` has the role `Math` and both `$` are hidden
- When the caret is on the line
- Then both `$` are shown with the role `MathMarker`

**Scenario: dollars as currency stay text**
- Given `If an item costs $5.00 and another costs $10.00, does …`
- Then nothing on the line is math
- And in `\$5.00 and \$10.00, vs real inline math $x = 5.00 + 10.00$` only `x = 5.00 + 10.00` is
  math and the backslashes are concealed (S020)

**Scenario: the rules of a closing dollar**
- Given `$ x$`, `$x $`, `$x$1`, `$$`, `$$$$` and `` `$x$` ``
- Then none of them holds math (the last one is inline code)
- And in `$a $b$` the `$` after the blank cannot close, so the formula is `a $b` (as Pandoc reads
  it)

**Scenario: the currency line of the sample as a whole**
- Given line 6 of the sample, with the code span `` `5.00 and another costs` `` between the amounts
  and the escaped amounts
- Then only `x = 5.00 + 10.00` is math: a formula never spans a code span (decision 3), and an
  escaped `$` neither opens nor closes one

**Scenario: Markdown inside a formula stays as written**
- Given `$a_1 * b_2 * c$` and `$\{x\}$`
- Then neither has emphasis, and no backslash in the formula is concealed

**Scenario: display math on one line**
- Given `$$\sum_{k=1}^n k$$`
- Then the content is math, marked as display in `math`, and the `$$` on each side are concealed

**Scenario: a display block**
- Given the Cauchy-Schwarz block of the sample (`$$`, the formula, `$$`) with the caret elsewhere
- Then the three lines are a math block, the formula line has the role `Math`, and the `$$` lines
  are hidden
- When the caret is on a `$$` line
- Then that line shows its `$$` with the role `MathMarker`

**Scenario: math in a code block is code**
- Given a fenced code block holding `$$` and `$x$`
- Then its lines are code, not math

**Scenario: theming**
- Given a theme that sets `math.text`, `math.marker`, `math.font-family` and `math.font-scale`
- Then formula source and revealed delimiters use them; without them they fall back as the table
  below says (ADR 0004)

## Theme tokens

| Token | Visible property | Fallback |
|---|---|---|
| `math.text` | colour of formula source (S046/S047: typeset formulas) | `colors.text` |
| `math.marker` | revealed `$` and `$$` | `colors.marker` |
| `math.font-family` | font of formula source; "" = editor font (S045 adds the math font) | "" |
| `math.font-scale` | size relative to the editor font, 0.5 to 2 | 1 |

S045–S047 add the tokens of typeset formulas (error colour, display panel).

## Out of scope (here)

Typesetting (S045–S047); `\(…\)` and `\[…\]` delimiters; `\begin{equation}` without `$$`;
macros; `mhchem`.

## Tests

- Core (`tests/core/test_s044_math_source.cpp`, `S044_MathSource`): styling and concealment of
  inline math, revealed markers, currency, the sample's currency line, the closing-dollar rules,
  Markdown inside formulas, emphasis beside a formula, display math on one line, display blocks
  (pure and with text beside `$$`, revealed delimiter lines, unclosed, in a code block, indented
  four spaces), every formula of the sample, the `math.*` tokens and their fallbacks; `S005_Theme`
  covers the new tokens' documentation and fallbacks.
- UI (`tst_s044_math_source`): formats from `math.text`, `math.font-scale` and `math.marker`,
  concealed dollars, display block lines, the sample saved byte for byte.

## Implementation

- `src/core/src/elements/math.cpp`: `InlineMathElement` (after inline code, before emphasis and
  escapes) collects the dollar runs of a line and finds every opener's closer in one right-to-left
  pass, so a line stays linear; an opaque character between two runs is a barrier.
  `DisplayMathElement` (state 1 while open) opens on `$$` with up to three spaces and no second
  `$$`, closes on a line ending in an unescaped `$$`.
- `LineRender`: roles `Math`, `MathMarker`; `math` (`MathSpan`), `mathBlock`, `mathDelimiter`.
- `EditorFormats::applyMath`, `applyMathMarker`; `MathTokens`, `MathStyle`, `mathStyle()`.
- Until S047, a revealed display block reveals its delimiter lines one by one (the caret's line),
  not the whole block as fences do (S021); the whole block reveals with its embedding in S047.
