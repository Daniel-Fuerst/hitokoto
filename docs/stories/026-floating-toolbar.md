# S026 — Floating toolbar on the right-hand side

- **Status:** In review · **Order:** 27 · **Size:** S · **Depends on:** S015, S018
- **Basis:** User request and confirmation on 2026-11-03: a vertical floating toolbar centered on
  the right edge of the editor with two entries: "Create new Table" (inserts a 2-column × 1-body-row
  table and focuses the first header cell) and "Create inline-codeblock" (wraps the selection in
  backticks or inserts `` `` `` with the caret inside).

## Story

As a writer, I want a floating toolbar on the right-hand side of the editor with buttons to create a
new table and an inline code span, so that I can insert them with one click without leaving the
editor.

## Acceptance scenarios

**Scenario: floating toolbar is shown on the right-hand side**
- Given an open editor window
- Then a vertical floating toolbar is visible, centered vertically along the right edge of the
  editor at `toolbar.margin` from the edge
- And it stays in place when the document is scrolled or the window is resized
- And its buttons have tooltips and accessible names ("Create new table", "Create inline code") and
  do not steal keyboard focus from the editor when clicked

**Scenario: creating a new table from the toolbar**
- Given the caret on an empty line in the editor
- When I click "Create new Table" in the floating toolbar
- Then a 2-column × 1-body-row GFM pipe table (`|  |  |\n| --- | --- |\n|  |  |`) is inserted at
  that line, shown immediately as an embedded table view, and the first header cell `(0, 0)` has
  focus
- Given the caret on a non-empty line in the editor
- When I click "Create new Table" in the floating toolbar
- Then the table is inserted on a new line below the current line (with a blank separator line
  before any following text so following text stays a paragraph), shown as an embedded table view,
  and the first header cell `(0, 0)` has focus
- And `Ctrl+Z` undoes the table insertion in one step

**Scenario: creating inline code from the toolbar**
- Given the caret in the editor with no selection
- When I click "Create inline-codeblock" in the floating toolbar
- Then `` `` `` is inserted at the caret and the caret stands between the two backticks, with the
  editor keeping focus
- Given selected text in the editor
- When I click "Create inline-codeblock" in the floating toolbar
- Then the selected text is wrapped in single backticks (`` `selected` ``) in one undo step

**Scenario: theming baseline**
- Given a theme with `[toolbar]` tokens (`background`, `border`, `border-width`, `radius`,
  `padding`, `spacing`, `margin`, `icon`, `button-size`, `button-radius`, `hover-background`,
  `active-background`, `active-icon`)
- When the theme is applied
- Then the floating toolbar and its buttons use those tokens, falling back to the documented general
  tokens when unset

