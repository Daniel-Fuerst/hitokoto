# S018 — Markdown tables

- **Status:** In review · **Order:** 20 · **Depends on:** S015, S019
- **Basis:** User request and confirmation on 2026-10-26; ADR 0005. Revised on 2026-10-27 after the
  code review (`docs/reviews/018-tables-code.md`) for the editor surface of S019: the table is an
  embedded Qt table view; GFM rules throughout; line breaks in cells; a source view. The revision
  was confirmed by the user on 2026-10-27. Amended on 2026-10-27 after Daniel's test recording:
  typing a table, editing with one click, the controls' place and every column visible (confirmed
  by the user on 2026-10-27). Amended again on 2026-10-27: the editing behaviour of the first
  version (Daniel, before the revision) is restored where the revision had lost it; see "Kept from
  the first version" (user decision on 2026-10-27). Amended on 2026-10-27 after the user's test:
  spaces at the edges of a cell while typing, and typing in the blank line below a table starts a
  paragraph (user decision on 2026-10-27). Amended again on 2026-10-27: files whose text follows a
  table without a blank line (as Obsidian writes them) get the blank line when opened (user
  decision; ADR 0002, decision 6). Amended again on 2026-10-27: the blank line that separates a
  table from the text below it is syntax and is hidden like the table's other syntax (user
  decision). The status stays "In review": the user's test of the changes completes the review.
- **Review basis:** Linux only (CI on `feature/tables` with Qt 6.12 and Qt 6.8.3; the Windows and
  macOS jobs are disabled in `ci.yml`), as for S019 by the user's decision of 2026-10-27; Windows
  and macOS run with the pull request to `main`.
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
- And the table is as wide as its columns: every column is visible without horizontal
  scrolling unless the table is wider than the text column, also after adding a column

**Scenario: typing a table in the editor**
- Given the caret in a line I type in the editor
- When that line becomes part of a table (header or delimiter row)
- Then the table shows its Markdown lines while the caret stays in that line, so I can finish
  typing it
- And the table view appears once the caret leaves the line (`Enter`, arrow keys, a click),
  with the caret where I moved it
- And a line that is already part of the shown table does not switch it to its Markdown lines;
  `Backspace` below a table moves the caret to the end of the last cell, where I type on (as in
  the first version; see "the blank line below a table")
- And keys always reach the editor or a cell: when a table switches to its Markdown lines while
  the caret is in it, the caret goes into those lines

**Scenario: cells show inline Markdown and line breaks**
- Given cells with `**bold**`, `*italic*`, `~~strike~~`, `` `code` ``, `\|` and `<br>`
- When the table is shown
- Then each cell shows the formatted text (escaped pipes as `|`, also in code), `<br>` as a line
  break, and long text wraps inside its column

**Scenario: editing a cell changes only that cell**
- Given a table
- When I click a cell once (the caret goes where I clicked, also in an empty cell), or move into
  it, and type, delete, paste or press `Shift+Enter`
- Then every keystroke changes that cell in its Markdown line at once and nowhere else in the
  file: the document is marked modified, saving writes it, and `Ctrl+Z` undoes it as typing
  elsewhere; there is no separate editing mode to confirm or cancel
- And the caret is always in the text of a cell: `Backspace` at the start of a cell and `Delete`
  at its end change nothing, and an escaped pipe is deleted and stepped over as one character
- And `Shift+Enter` and line breaks in pasted text are written as `<br>`, and every `|` typed or
  pasted is written as `\|`, so the row keeps its cells
- And a space typed at the start or end of a cell's text stays in the cell while I type; GFM
  treats spaces at a cell's edges as padding, so the file gets it with the next character
- And I can select text in a cell by dragging with the mouse or with `Shift` and the arrow keys,
  `Home` or `End`
- And the context menu of a cell has `Cut`, `Copy` and `Paste` for the cell's text and `Add Row`,
  `Delete Row`, `Add Column`, `Delete Column` and `Show Markdown Source`

**Scenario: moving with the keyboard**
- Given the caret in a cell
- When I press `Tab` or `Shift+Tab`
- Then the caret moves to the next or previous cell, from the end of a row to the next row and
  back, skipping the delimiter row
- When I press `Left` at the start of a cell's text or `Right` at its end
- Then the caret moves to the end of the previous cell or the start of the next one, across rows;
  from the first cell of the table to the end of the line above it, from the last cell to the
  line below it
- When I press `Up` or `Down`
- Then the caret moves line by line within the wrapped text of the cell first, then to the same
  column of the row above or below; from the header row to the line above the table, from the
  last row to the line below it
- When I press `Home` or `End`
- Then the caret moves to the start or end of the cell's text (with `Shift`, selecting)
- When I press `Enter` in a row that is not the last one
- Then the caret moves to the same column of the next row

**Scenario: moving around never changes the file**
- Given a table, including rows with fewer cells than the header
- When I move with `Tab`, `Shift+Tab`, arrow keys, `Home`, `End` or clicks, select cells, or
  scroll
- Then the file is unchanged and the document is not marked modified
- And `Down` or `Up` from the line above or below the table move into the table

**Scenario: adding and removing rows and columns**
- Given a table
- Then add-row (`+`), add-column (`+`), delete-row (`−`) and delete-column (`−`) controls appear
  only while the pointer is near them, each along the edge it acts on: add-row as a bar under the
  table and add-column as a bar to its right, as wide and as high as the table; delete-row left
  of its row and delete-column above its column, as high or as wide as they are (drawn in the
  table's header colours, as before the revision), and the context menu has `Add Row`, `Delete Row`,
  `Add Column`, `Delete Column`
- When I use them, or press `Tab` in the last cell, or `Enter` in the last row while it is not
  empty
- Then rows and columns are added to or removed from the header, delimiter and body lines, the
  caret moves into the new or a neighbouring cell, and `Ctrl+Z` restores the previous lines
- And `Backspace` or `Delete` in an empty body row deletes that row (`Backspace` moves the caret
  to the end of the row above, `Delete` to the row now in its place); in a cell of an empty
  column (every cell of it empty, header included) they delete that column, unless it is the last
  one
- And `Ctrl+Shift+Backspace` or `Ctrl+Shift+Delete` deletes the caret's body row, or in the header
  row the caret's column; `Ctrl+Alt+Backspace` or `Ctrl+Alt+Delete` deletes the caret's column
  from any row; the last column is never deleted

**Scenario: writing below a table**
- Given the blank line directly below a table (GFM would make text there a row of the table)
- When I type, paste or commit input method text in it
- Then hitokoto first adds a blank line, so the text is a paragraph below the table, and the
  table stays a table

**Scenario: the blank line below a table**
- Given a table followed by a blank line (GFM needs it before text that is not a row)
- Then that one blank line is not shown: the next line follows the table at the distance of the
  add-row bar's space, as close as a paragraph follows a paragraph; further blank lines show as
  blank lines, as everywhere in the document
- And the caret never lands on the hidden line: `Down` in the last row, `Right` in the last cell
  and a click below the table go to the line after it (or onto a new line, see "leaving the
  table"); `Up` and `Left` from that line go into the table. Where no line follows it, and right
  after I pressed `Enter` below a table I typed, the caret is on that line, which then shows
  until the caret leaves it (moving around never changes the file)
- And `Backspace` at the start of the line after it moves the caret to the end of the last cell
  and deletes nothing when the line has text; an empty line is removed, as in any text
- And the file keeps the blank line, and the source view (`Ctrl+Shift+M`) shows it
- And deleting a selection that includes the hidden line removes it like any text

**Scenario: files from other editors**
- Given a file in which a line without `|` directly follows a table's rows (Obsidian writes this
  and shows the line as a paragraph; GFM and GitHub make it a row of the table)
- When I open it
- Then hitokoto adds a blank line before that line, so it is a paragraph in hitokoto, Obsidian
  and on GitHub alike
- And the document is marked modified and a message says how many blank lines were added; the
  file on disk changes only when I save, and closing without saving leaves it as it was
- And a line with `|` after a table, text after a blank line, and anything inside a code block
  stay as they are

**Scenario: leaving the table**
- Given the caret in a cell
- When I press `Escape` or `Ctrl+Enter`, or `Enter` in an empty last row (which removes that row)
- Then the caret moves below the table, and what I typed stays
- When I click below a table that ends the document, or above a table that starts it
- Then the caret moves onto a new line there
- When I press `Up` in the header row or `Left` in the first cell of a table that starts the
  document
- Then the caret moves onto a new line above it
- And a new line below or above a table is separated from it by a blank line, so text typed there
  is a paragraph and not a table row (GFM continues a table on the next line); below the table
  the caret goes to the line after an existing blank line, and lines are added only if there is
  none (the only changes leaving a table makes)

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
- Cells drawn by Qt's Markdown renderer. The caret is always in a cell's text: a Qt text editor
  in the current cell writes every keystroke to the Markdown line at once.
- S009 gains a table scenario (caret, input method and screen reader in cells).
- Not in scope: multi-line block content in cells (lists, code blocks), sorting, column resizing by
  dragging.

## Kept from the first version

The revision of 2026-10-27 rebuilt the table on an embedded table view and lost behaviour of the
first version (Daniel, commit 620b6cb: `TableController`, `tests/ui/tst_s018_tables.cpp`) that
the story did not spell out. The behaviour inventory of 2026-10-27 compared the two; the user
decided to restore points 11 to 21, now in the scenarios above:

| # | Behaviour of the first version | Scenario |
|---|---|---|
| 11 | Every keystroke writes to the Markdown line at once; no editing mode | Editing a cell |
| 12 | `Escape` leaves the table below, keeping what was typed | Leaving the table |
| 13 | Leaving at the end of the document adds a blank line before the new line | Leaving the table |
| 14 | `Left` and `Right` at the edge of a cell's text move to the neighbouring cell, at the edge of the table out of it | Moving with the keyboard |
| 15 | `Up` and `Down` move through wrapped cell text first, then to the row above or below | Moving with the keyboard |
| 16 | `Backspace` or `Delete` in an empty row or column deletes it | Adding and removing |
| 17 | `Ctrl+Shift+Backspace`/`Delete` deletes the row (in the header: the column), `Ctrl+Alt+Backspace`/`Delete` the column | Adding and removing |
| 18 | A click below a table at the end or above a table at the start of the document adds a line there | Leaving the table |
| 19 | Leaving upwards from a table at the start of the document adds a line above it | Leaving the table |
| 20 | The context menu has the text actions and the table actions | Editing a cell |
| 21 | Text in a cell can be selected with the mouse | Editing a cell |

Not restored, by the revision's decisions: navigation pads rows that lack cells (review objection
O6: moving around never changes the file); pipes inside code spans do not split cells (O1, GFM).

## Implementation

On `feature/tables`, 2026-10-27, after the code review of the first version
(`docs/reviews/018-tables-code.md`, all twelve objections fixed):

- Core: table recognition follows GFM (cmark-gfm): pipes in code spans split cells, a line
  without a pipe continues the table, up to 255 columns; cell text with `<br>` and `\|` in both
  directions.
- UI: `TableEmbedder` shows each table as a `TableWidget` (a `QTableView` with hover controls)
  over the space its header line reserves in `BlockLayout` (S019); `TableModel` reads and writes
  the cells in the Markdown lines. The current cell's editor stays open and writes every
  keystroke at once; the keys of the first version work across cells and out of the table.
- Robustness: a table whose lines a change removes is never touched again and is rebuilt; tables
  are not looked for in the middle of a document change (crashes found by the CI benchmark and on
  Qt 6.8.3).
- Files from other editors (ADR 0002, decision 6): `core::separateTextAfterTables` adds the
  blank lines on the loading thread right after decoding; `DocumentSession` then marks the
  document modified and `MainWindow` says how many lines were added.
- The blank line below a table: `TableEmbedder` hides it with the table's lines while the view
  shows the table and the caret is not on it; keys and caret moves go over it.
- Budgets on the development VM: opening a 1000-row table 110–130 ms (500 ms), typing in a
  200-row table p95 about 4 ms, longest keystroke under 10 ms (50 ms hard).

Next: the user's acceptance ("Done"); the benchmark baselines gain the `s018_*` values from a CI
run; Windows and macOS with the pull request to `main`.

## Tests

- Core: `S018_Tables` for the GFM rules (checked against cmark-gfm), cell text with `<br>` and
  `\|` in both directions, row and column helpers, theme tokens.
- UI: `tst_s018_tables` for every scenario above, including "moving around never changes the
  file" and the source view; the cases of the first version's tests at 620b6cb for points 11 to
  21 (navigation, deleting with keys, leaving at the start and end of the document, the empty
  rows' height) are carried over.
- UI, for the blank line below a table: it is hidden and the next line follows at the add-row
  bar's distance; a second blank line stays visible; `Down`, `Right`, `Up`, `Left` and clicks skip
  it; `Backspace` on a line with text and on an empty line; the source view shows it; the file is
  unchanged.
- Benchmark: `s018_table_typing_*` and `s018_table_open_ms` in `hitokoto-bench`.
