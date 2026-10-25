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

## Outcome

**ADR 0003 – concealment technique**: keep the `QPlainTextEdit` adapter, or switch to the
custom `QAbstractScrollArea` + `QTextLayout` surface (plan B). Core does not change either way.

## Tests

- UI: `tst_s009_concealment` – residue measurement via `cursorRect`, randomised caret walks.
- IME and screen-reader checks are manual, documented with platform and version in the ADR.
