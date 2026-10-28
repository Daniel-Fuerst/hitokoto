# S009 — Concealment quality gate: caret, IME, accessibility

- **Status:** Standing quality gate · **Order:** outside the backlog order · **Depends on:** S003

This is a cross-cutting condition rather than a one-off story. Its scenarios are extended whenever
an element conceals syntax, and it is checked again then; it may leave the backlog order
(decided 2026-10-25).

## Story

As the project owner, I want to know early whether hiding markers with the 1 px technique
(ADR 0001) is good enough, so that we switch to the custom text surface before more elements
depend on it.

## Acceptance scenarios

**Scenario: no visible residue**
- Given headings of levels 1–6, bullets, emphasis delimiters (S013), code-span backticks (S015)
  and code fences (S014), with the caret elsewhere
- Then the visible text starts at the same x position as an unmarked line (tolerance 0.5 px)

**Scenario: caret never inside hidden text**
- Given arbitrary sequences of Up/Down/Left/Right/Home/End/PageUp/PageDown and clicks
- Then the caret is never placed inside a concealed range

**Scenario: IME composition on a heading line**
- Given a Japanese or Chinese input method on each platform
- When I compose text on a heading line and in the middle of a bullet item
- Then the pre-edit text appears at the caret and the committed text lands at the right position

**Scenario: screen reader**
- Given NVDA (Windows), VoiceOver (macOS) and Orca (Linux)
- When I read a heading line with the caret elsewhere and on it
- Then the text is read sensibly (markers either skipped or read as Markdown, but consistently)

**Scenario: code block fences (S021)**
- Given code blocks whose fences are concealed with their space kept (transparent text), with and
  without an info string, a tilde fence and a block that runs to the end of the document
- When the caret walks with arbitrary sequences of Up/Down/Left/Right/Home/End/PageUp/PageDown
- Then whenever the caret is on a line of a block, both fences of that block are revealed, so the
  caret never stands in concealed text
- And no line of the block moves or changes height compared with the concealed state

**Scenario: table cells (S018)**
- Given a table shown as a table view, with the caret moving into it from the line above and below
- When I move between cells, compose text with an input method in a cell and read the table with
  a screen reader
- Then the caret is only ever in a cell or on a visible line, the pre-edit appears in the cell and
  the committed text lands there, and the screen reader reads the cells (Qt's table view) instead
  of pipes

## Outcome

**ADR 0003 – concealment technique**: keep the `QPlainTextEdit` adapter, or switch to the
custom `QAbstractScrollArea` + `QTextLayout` surface (plan B). Core does not change either way.
Decided on 2026-10-27 for the surface (S019): `QTextEdit` with our own document layout. The 1-px
concealment stays; this gate keeps checking it on the new surface.

## Tests

- UI: `tst_s009_concealment` – randomised caret walks (since 2026-10-28, with S021: headings,
  bullets, emphasis, inline code and code blocks; the caret never in concealed text, code block
  fences revealed and the block still). Residue measurement via `cursorRect` is still to come.
- IME and screen-reader checks are manual, documented with platform and version in the ADR.
