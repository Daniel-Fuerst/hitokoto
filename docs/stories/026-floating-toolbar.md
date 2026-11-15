# S026 — Floating toolbar on the right-hand side

- **Status:** In review · **Order:** 27 · **Size:** S · **Depends on:** S015, S018
- **Basis:** User request and confirmation on 2026-11-03: a vertical floating toolbar centered on
  the right edge of the editor with two entries: "Create new Table" (inserts a 2-column × 1-body-row
  table and focuses the first header cell) and "Create inline-codeblock" (wraps the selection in
  backticks or inserts `` `` `` with the caret inside).
  Extended on 2026-11-15: replaces the top menu bar (`File`, `Edit`, `View`) with navigator entries
  for **New**, **Open**, **Save**, and **Find** on the floating toolbar, while standard shortcuts
  remain active as window actions on `MainWindow`.

## Story

As a writer, I want a floating navigator toolbar on the right-hand side of the editor with buttons
for New, Open, Save, Find, creating a new table, and creating an inline code span, so that I can
navigate and insert without a top menu bar and without leaving the editor.

## Acceptance scenarios

**Scenario: floating toolbar is shown on the right-hand side**
- Given an open editor window
- Then no top menu bar (`QMenuBar`) is shown
- And a vertical floating toolbar is visible, centered vertically along the right edge of the
  editor at `toolbar.margin` from the edge
- And it stays in place when the document is scrolled or the window is resized
- And its buttons have tooltips and accessible names ("New", "Open", "Save", "Find",
  "Create new table", "Create inline code") and do not steal keyboard focus from the editor when
  clicked

**Scenario: New, Open, Save, and Find from the toolbar**
- Given an open editor window
- When I click "New", "Open", "Save", or "Find" in the floating toolbar
- Then "New" starts an empty untitled document (asking to save unsaved changes first)
- And "Open" picks a file and opens it (asking to save unsaved changes first)
- And "Save" saves the document (or asks for a path when untitled)
- And "Find" opens the find bar below the editor and focuses the search field
- And all standard keyboard shortcuts (`New`, `Open`, `Save`, `Save As`, `Quit`, `Undo`, `Redo`,
  `Cut`, `Copy`, `Paste`, `Paste as Plain Text`, `Select All`, `Find`, `Find Next`,
  `Find Previous`, `Reload Theme`) remain active on `MainWindow`

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

