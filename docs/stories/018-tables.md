# S018 — Markdown tables

- **Status:** In progress · **Order:** 20 · **Depends on:** S015, S019
- **Basis:** User request and confirmation on 2026-10-26; ADR 0005. Revised on 2026-10-27 after the
  code review (`docs/reviews/018-tables-code.md`) for the editor surface of S019: the table is an
  embedded Qt table view; GFM rules throughout; line breaks in cells; a source view. The revision
  was confirmed by the user on 2026-10-27.
- **Renumbered:** from S017 on 2026-10-27, because S017 is "Saving never stalls the UI" on main.
  Commits 8130875 and a3def18 still say S017.

## Story

As a writer, I want Markdown pipe tables to show as a table in which I write directly in the
cells, with line breaks, and add rows and columns graphically, while the file stays plain GFM
Markdown that GitHub shows the same way.

## Acceptance scenarios

**Scenario: GFM pipe tables show as a table**
- Given a header row, a delimiter row with the same number of cells (`| --- | :---: | ---: |`)
  and zero or more body rows
- When the document is shown
- Then the lines show as one table with the header row set apart, columns aligned to shared
  widths and each column aligned left, centred or right as the delimiter row says (for every
  column, however many there are)
- And the table follows GFM: every unescaped `|` separates cells, also inside a code span; a
  following line without a pipe is still a row; the table ends at a blank line or the start of
  another block; missing cells are empty; a table may directly follow a paragraph
- And the file still contains exactly the lines it had

**Scenario: cells show inline Markdown and line breaks**
- Given cells with `**bold**`, `*italic*`, `~~strike~~`, `` `code` ``, `\|` and `<br>`
- When the table is shown
- Then each cell shows the formatted text (escaped pipes as `|`, also in code), `<br>` as a line
  break, and long text wraps inside its column

**Scenario: editing a cell changes only that cell**
- Given a table
- When I click a cell, or move into it, and type, delete, paste or press `Shift+Enter`
- Then the text changes in that cell of that row and nowhere else in the file
- And `Shift+Enter` and line breaks in pasted text are written as `<br>`, and every `|` typed or
  pasted is written as `\|`, so the row keeps its cells
- And `Ctrl+Z` undoes the change in the document

**Scenario: moving around never changes the file**
- Given a table, including rows with fewer cells than the header
- When I move with `Tab`, `Shift+Tab`, arrow keys, `Home`, `End` or clicks, select cells, or
  scroll
- Then the file is unchanged and the document is not marked modified
- And `Up` from the header row and `Down` from the last row move to the line above or below
  the table, and `Down` or `Up` from there move into the table

**Scenario: adding and removing rows and columns**
- Given a table
- Then add-row (`+`), add-column (`+`), delete-row (`−`) and delete-column (`−`) controls appear
  only while the pointer is near them, and the context menu has `Add Row`, `Delete Row`,
  `Add Column`, `Delete Column`
- When I use them, or press `Tab` in the last cell, or `Enter` in the last row while it is not
  empty
- Then rows and columns are added to or removed from the header, delimiter and body lines, the
  caret moves into the new or a neighbouring cell, and `Ctrl+Z` restores the previous lines
- And `Enter` in an empty last row removes it and moves below the table; `Escape` and
  `Ctrl+Enter` move below the table; if the table ends the document, an empty line is added
  for the caret (the only change leaving the table makes)

**Scenario: the source view shows everything**
- Given a table with text the table does not show (cells beyond the header's column count,
  text after the last pipe), marked in the table view at its row
- When I press `Ctrl+Shift+M` or choose `Show Markdown Source` in the context menu
- Then that table shows its Markdown lines, editable like any text, until I press the shortcut
  or choose `Show Table` again; the caret stays in the same cell or line

**Scenario: non-tables stay literal**
- Given a line containing `|` without a matching delimiter row below it
- Then it is shown as ordinary text with its pipes visible

**Scenario: tables are themeable (theming baseline, ADR 0004 and ADR 0005)**
- Given a theme that sets every `table.*` token (`background`, `header-background`,
  `stripe-background`, `text`, `header-text`, `header-weight`, `border`, `border-width`, `radius`,
  `padding`, `font-family`, `font-scale`)
- Then the table, its header, alternating rows, grid, cell padding and cell text use them
- And a theme that sets none of them falls back as documented in `docs/theming.md`

**Scenario: tables keep the budgets**
- Given a document with a 200-row table and one with a 1000-row table
- When I type in a cell, and when I open the files
- Then typing stays within the typing budget (p95 ≤ 16 ms, no keystroke over 50 ms) and opening
  within the open budget; a benchmark measures both

## Scope

- GFM 0.29 pipe tables (spec section 4.10), checked against cmark-gfm.
- The table view (`QTableView`) embedded in the editor through `BlockLayout` (S019): the header
  line reserves the view's height, the other lines are hidden; the Markdown lines stay the only
  copy of the text (ADR 0002).
- Cells drawn by Qt's Markdown renderer; cells edited in a Qt text editor.
- S009 gains a table scenario (caret, input method and screen reader in cells).
- Not in scope: multi-line block content in cells (lists, code blocks), sorting, column resizing by
  dragging.

## Tests

- Core: `S018_Tables` for the GFM rules (checked against cmark-gfm), cell text with `<br>` and
  `\|` in both directions, row and column helpers, theme tokens.
- UI: `tst_s018_tables` for every scenario above, including "moving around never changes the
  file" and the source view.
- Benchmark: `s018_table_typing_*` and `s018_table_open_ms` in `hitokoto-bench`.
