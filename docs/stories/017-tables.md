# S017 — Markdown tables

- **Status:** In review · **Order:** 18 · **Depends on:** S015
- **Basis:** User request and confirmation on 2026-10-26; ADR 0005.

## Story

As a writer, I want Markdown pipe tables to render as an interactive table where I write directly in
the cells and add rows and columns graphically without seeing raw Markdown table syntax, while the
document remains plain GFM Markdown.

## Acceptance scenarios

**Scenario: GFM pipe tables render as an aligned grid with syntax concealed**
- Given a header row of pipe-separated cells followed by a matching delimiter row (`| --- | :---: | ---: |`)
  and zero or more body rows
- When the table is rendered, whether the caret is outside or inside the table
- Then pipe delimiters (`|`), cell-padding whitespace, escaped-pipe backslashes (`\|`) and the
  delimiter row are concealed
- And columns are aligned to shared widths across all rows of the table
- And cells in each column are aligned left (`---` or `:---`), centred (`:---:`) or right (`---:`)
  according to the delimiter row
- And the document still contains the plain Markdown source

**Scenario: editing directly inside table cells**
- Given a rendered table
- When the user clicks a cell, navigates between cells with `Tab`, `Shift+Tab`, `Enter` or arrow
  keys, and types or deletes text
- Then the caret lands inside the cell content, never on concealed pipe syntax or the delimiter row
- And typing updates that cell in the underlying Markdown row
- And `Backspace` at the start of a cell and `Delete` at the end of a cell do not delete the pipe
  delimiters
- And typing `|` inside a cell inserts an escaped pipe `\|` so the cell is not split

**Scenario: adding rows and columns graphically and via keyboard, and exiting the table**
- Given a rendered table
- Then the graphical add-row (`+`) and add-column (`+`) controls remain hidden unless the pointer
  hovers near the respective control
- When the user hovers near the bottom edge of the table and clicks the revealed add-row (`+`)
  control, or presses `Tab` in the last cell of the table (or `Enter` on a non-empty last row)
- Then a new empty Markdown row with matching columns is appended to the table and the caret moves
  into the new row
- And when the user presses `Enter` on an already-empty last body row
- Then that empty row is removed and the caret moves onto a plain paragraph line below the table
- And when the user clicks below the table (or presses `Ctrl+Enter` or `Escape` inside the table)
- Then the caret moves onto a plain line below the table even when the table was at the end of the
  document
- And when the user hovers near the right edge of the table and clicks the revealed add-column (`+`)
  control
- Then a new column is appended to the header row, delimiter row and every body row in the Markdown
  source, and the caret moves into the new column's header cell
- And when the user hovers near the left edge of a body row and clicks the revealed delete-row (`−`)
  control (or presses `Backspace` / `Delete` on an empty body row, presses `Ctrl+Shift+Backspace`,
  or selects `Delete Row` from the context menu)
- Then that body row is removed from the Markdown source and the caret stays inside an adjacent
  table row
- And when the user hovers near the top edge of a column header and clicks the revealed
  delete-column (`−`) control (or presses `Backspace` / `Delete` in an empty column, presses
  `Ctrl+Shift+Backspace` on a header cell or `Ctrl+Alt+Backspace` on any cell, or selects
  `Delete Column` from the context menu)
- Then that column is removed from the header row, delimiter row and every body row in the Markdown
  source

**Scenario: inline Markdown and escaped pipes inside cells**
- Given cells containing emphasis (`**bold**`, `*italic*`, `~~strikethrough~~`), inline code
  (`` `code | with | pipes` ``) or escaped pipes (`\|`)
- When the table is rendered
- Then inline elements are parsed independently per cell (delimiters never match across cell
  boundaries, and pipes inside inline code or after `\` do not split cells)
- And the visible text of `\|` is a literal `|`

**Scenario: non-tables stay literal**
- Given a line containing `|` that is not followed by a matching delimiter row (or whose delimiter
  row has a different column count)
- When the line is rendered
- Then it is not rendered as a table and its pipes remain visible

**Scenario: tables are themeable (theming baseline, ADR 0004 & ADR 0005)**
- Given a theme that sets every `table.*` token (`background`, `header-background`,
  `stripe-background`, `text`, `header-text`, `header-weight`, `border`, `border-width`, `radius`,
  `padding`, `font-family`, `font-scale`)
- When a table is rendered
- Then the table panel, header row, alternating body rows, grid borders, cell padding and text
  formats use those tokens
- And a theme that sets none of them falls back to `colors.background`, `colors.surface`,
  `table.background`, `colors.text`, `table.text`, `headings.weight`, `colors.border`, 1 px border,
  `layout.radius`, `layout.spacing`, the editor font family and scale 1

## Scope

- GitHub Flavored Markdown (GFM 4.10) pipe tables: a header row and a delimiter row with matching
  column counts, followed by zero or more pipe-table body rows.
- Interactive cell editing, cell navigation (`Tab`, `Shift+Tab`, `Enter`, arrows, clicks) and
  hover-revealed graphical `+` / `−` controls to append or delete rows and columns.
- Inline Markdown (`S013`, `S015`) inside cells, parsed per cell.

## Tests

- Core: `S017_Tables` verifies table recognition, 1-line header lookahead, delimiter alignments,
  cell spans, escaped pipes, pipes inside inline code, per-cell inline formatting, row/column
  manipulation helpers, and `[table]` theme token parsing and fallbacks.
- UI: `tst_s017_tables` verifies permanent concealment of table syntax even when focused, column
  width alignment and left/centre/right cell alignment, cell typing and pipe-protected deletion,
  `Tab`/`Shift+Tab`/`Enter`/arrow navigation, hover-only reveal and clicking of the graphical
  add-row/add-column (`+`) and delete-row/delete-column (`−`) buttons, keyboard row and column
  deletion (`Backspace`/`Delete` on empty rows/columns and `Ctrl+Shift+Backspace` /
  `Ctrl+Alt+Backspace`), and pixel/format checks for every `table.*` theme token.
