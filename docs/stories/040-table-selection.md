# S040 — Selecting cells, rows and columns in a table

- **Status:** Draft · **Order:** 45 · **Size:** L · **Depends on:** S018, S019, S028, S031
- **Basis:** Armin, 2026-11-21: a selection of more than one cell cannot be seen; rows and columns
  cannot be selected, or their selection is not visible. Checked on main `c8a167f` with Qt 6.12.0
  (Linux, offscreen) before this story was written:
  - The table view allows one cell only (`QAbstractItemView::SingleSelection`). `Shift` with a
    click on another cell acts as a plain click; dragging from a cell to another selects text in
    the first cell only; `Shift+Right` at the end of a cell's text and `Shift+Down` on its last line
    do nothing.
  - A selection would not be seen either: three cells selected directly in the view's selection
    model are painted exactly as before (the cell delegate ignores the selected state).
    `QTableView::selectAll`, `selectRow` and `selectColumn` change nothing.
  - `Ctrl+A` in a cell selects the cell's text, also when pressed again; `Shift+Space` types a
    space; `Ctrl+Space` does nothing; `Meta+Space` types a space (Linux).
  - The context menu of a cell has Qt's text actions (`Select All` selects the cell's text), then
    the table actions of S018.
  - In the editor, `Shift+Down` from the line above a table extends the selection over all of the
    table's lines at once; dragging from above onto a cell stops at the table's start. Either way
    the table does not look selected, also not after `Ctrl+A`. Parts of a table can be selected as
    text only in its source view (`Ctrl+Shift+M`).
  - The current cell is marked with the palette's highlight at a fixed alpha, not with a theme
    token.
  - Copying in the editor offers Qt's formats for rich text: plain text, `text/html`,
    `text/markdown` and ODF. Obsidian prefers the HTML when it is there and makes each line a
    paragraph of its own, so a copied table with text before it arrives as loose lines; Qt's
    `text/markdown` puts a blank line between the table's lines, so it is no table either (Armin's
    clipboard samples on Windows, 2026-11-21; pasted as plain text, the same copy arrives as a
    table).
  - The height of a table view after cutting or deleting some of its lines was checked as well (a
    point found in the rework of S028 (2026-11-15)): it equals the height in a fresh window with the
    same text, also right after the change, after undo and after redo. Where it differed, the cut
    had taken the blank line below the table, so the line after it became a row (GFM); a fresh
    window adds a blank line there when the file is opened (S018, "files from other editors").
    That is a different point and is in `unrefined.md` ("Cutting the blank line below a table").

## Story

As a writer, I want to select any rectangle of cells, whole rows, whole columns or the whole table
the way I know it from a spreadsheet, and see at once which cells are selected, so that I can copy,
cut, paste and delete exactly what I see.

## Decisions (Armin, 2026-11-21)

1. **Any rectangle of cells.** A selection of cells is a rectangle: in a 4 x 4 table any 2 x 2 or
   3 x 2 block, adjacent rows together, adjacent columns together, or the whole table. It has a
   start cell, which stays fixed while the selection grows or shrinks, and an end cell at the
   opposite corner.
2. **Selected cells are visible as selected.** Every selected cell shows the selection colours; the
   end cell is set apart by a frame.
3. **`Ctrl+A` twice selects the table.** `Ctrl+A` in a cell selects the cell's text, as today. If
   the current cell is already selected as a whole (its whole text is selected, or the cell itself
   is selected as a cell, by `Ctrl+A`, by the mouse or by keys), `Ctrl+A` selects the whole table.
   So two presses of `Ctrl+A` with the caret in a table select the table. In an empty cell the first
   `Ctrl+A` selects the cell as a cell. A third `Ctrl+A`, with the whole table selected, selects the
   whole document, as `Ctrl+A` does outside a table.
4. **Keys, as in Excel and Calc.** On Windows and Linux `Shift+Space` selects the current row and
   `Ctrl+Space` the current column. On macOS as in Excel for Mac: `Shift+Space` the row,
   `Control+Space` (the Control key, not Command) the column, `Cmd+A` for `Ctrl+A`. With cells
   selected, `Shift+Space` selects the whole rows they lie in and `Ctrl+Space` the whole columns. A
   column includes the header cell. That `Shift+Space` no longer types a space in a cell is accepted
   (Armin: a habit, as in spreadsheets). While an input method composes text (pre-edit), these keys
   belong to it. Note for the implementation: Qt maps `Qt::ControlModifier` to Command on macOS; the
   column key there needs `Qt::MetaModifier` (the Control key), and `Ctrl+A` goes through
   `QKeySequence::SelectAll`.
5. **When the system takes a key.** Where the system or an input method uses `Ctrl+Space`,
   `Control+Space` or `Shift+Space` itself, the key never reaches hitokoto, and hitokoto offers no
   other key for it (Excel has the same overlap). Known cases: macOS selects the previous input
   source with `Control+Space` by default; the Chinese input methods of Windows switch with
   `Ctrl+Space`; fcitx on Linux switches with `Ctrl+Space` and, with a Chinese input method on,
   toggles full and half width with `Shift+Space`. Rows and columns stay selectable with the mouse
   (decision 6) and the context menu (decision 7).
6. **Mouse on rows and columns.** Every row has an area left of its `−` control (the control that
   deletes it), every column one above its `−` control; the header row and a table's only column,
   which have no `−` control, have the area in the same place. A click there selects the row or
   column, dragging across adjacent areas selects adjacent rows or columns, and `Shift` with a click
   extends from the start cell. The area is not drawn: over it, the mouse pointer shows it, an
   arrow pointing right over a row's area and an arrow pointing down over a column's area. The `−`
   controls stay where they are and appear as before; the areas lie beyond them, farther from the
   table, so the space around the table grows by the area's size on the left and at the top.
   The two pointers are drawn for hitokoto (no third-party images): a filled arrow with a straight
   shaft and a triangular head, the shaft about half as high as the head and the head about as
   long as the shaft; the down arrow is the same shape turned by 90 degrees. Details (adjustable
   later): black fill with a white outline of 1 px at 1x, as the system's own pointers, so that
   they show on light and dark tables; 32 x 32 px at 1x with versions for 2x, drawn as SVG in the
   repository and turned into a `QCursor` at the screen's scale through Qt SVG, which hitokoto
   links already (no image format plugin, S037); hot spot at the tip of the arrow.
7. **Context menu of the table.** `Select All` selects the whole table (matching its icon).
   Directly below it: `Select Column` and `Select Row`. Directly below `Copy`:
   `Copy as Tab-Separated Text` (decision 10). A right click inside a selection of cells keeps it;
   a right click on another cell ends it, and that cell becomes the current one.
8. **From text to cells.** A selection starts as text in the current cell and becomes a selection
   of cells at the edge of the cell's text:
   - `Shift+Right` selects up to the end of the text; the next `Shift+Right` selects the whole cell
     as a cell, wherever the text selection started; the next one adds the cell to the right, and
     from then on the rectangle grows and shrinks from the start cell. `Shift+Left` in the same way
     towards the start.
   - `Shift+Down` moves line by line through the cell's wrapped text; on its last line it selects
     up to the end of the text, as today; the next `Shift+Down` selects the whole cell; the next one
     adds the cell below. `Shift+Up` in the same way towards the start.
   - With cells selected, `Shift+Home` and `Shift+End` extend to the first or last column,
     `Ctrl+Shift+Home` and `Ctrl+Shift+End` to the first or last cell of the table. With text
     selected they act on the text, as today.
   - Dragging with the mouse selects text while the pointer stays over the text of the start cell;
     past the end of the cell's text it selects the whole cell; in another cell it selects the
     rectangle from the start cell to that cell; back over the text of the start cell it is a text
     selection again. `Shift` with a click on another cell selects the rectangle from the current
     cell to that cell.
9. **Ending a selection of cells.** An arrow key without `Shift`, `Tab` or a click ends it; the
   caret goes to the end cell, or where the click was. `Escape` ends it and leaves the caret in the
   end cell; only a second `Escape` leaves the table, as in S018. `Ctrl+Shift+M` ends it and shows
   the source with the caret on the end cell's line. A change of the table's lines that does not
   come from the selection (undo, redo, an edit through the find bar) ends it as well.
10. **Copy.** hitokoto's own copy offers Markdown only:
    - In the editor and from a cell's text: `text/plain` and `text/markdown`, both holding exactly
      the selected text of the file, and the marker `application/x-hitokoto-copy` of S031. No
      `text/html` and no ODF any more.
    - From selected cells: `Ctrl+C` puts the cells as a GFM table into `text/plain` and
      `text/markdown`, in S031's minimal form (`| a | b |`, delimiter `| --- |`, no padding). The
      first selected row is the header row; each column keeps its alignment from the delimiter row;
      each cell holds its Markdown as in the file (`**bold**`, `\|`, `<br>` unchanged). The whole
      table gives exactly its lines, without the blank line below it. A single cell selected as a
      cell gives its text, as copying its whole text does today. Line endings on the clipboard are
      LF.
    - `Copy as Tab-Separated Text` (context menu, enabled with cells selected) puts only
      `text/plain`, tab-separated in Excel's form, for spreadsheets: rows end with a line break,
      cells are separated by a tab, a field is quoted only when it holds a tab or a line break and
      `"` inside it is doubled. Each field is the text the cell shows: no formatting, `\|` as `|`,
      `<br>` as a line break.
11. **Paste prefers Markdown.** When the clipboard holds `text/markdown`, `Ctrl+V` in the editor
    pastes that Markdown as it is written; the clipboard's HTML and tab-separated text are not
    looked at. If it is exactly one GFM table, it lands as S031 places a table ("Where it lands":
    its own block, one undo step); any other Markdown is inserted as text. `Paste as Plain Text`
    still takes only `text/plain`. S031 and ADR 0011 get a dated note with the implementation.
12. **Delete, cut, paste and typing on selected cells.**
    - `Delete` and `Backspace` empty the selected cells, also when whole rows, whole columns or the
      whole table are selected; the rows and columns stay.
    - `Delete Row` and `Delete Column` (context menu and the keys of S018) remove all selected whole
      rows or columns at once; the header row and the last column stay, as in S018.
    - `Cut` copies as in decision 10 and empties the cells at once (as Calc does).
    - Paste with cells selected, or with the caret in a cell: a table on the clipboard
      (`text/markdown` holding a table, or an HTML or tab-separated table under S031's rules) fills
      the cells from the top-left selected cell, or from the cell with the caret; its header row is
      an ordinary row there; rows and columns are added as needed, as adding does in S018; cells
      outside the pasted block stay. Cells from `text/markdown` keep their Markdown; cells from HTML
      or tab-separated text follow S031's rules for cell text and escaping. Text that is no table
      replaces the content of every selected cell (with a cell's line breaks as `<br>` and `|` as
      `\|`, as in S018); with the caret in a cell it is pasted as today.
    - A typed character with cells selected replaces the text of the end cell and ends the
      selection; the other cells stay.
    - Each of these is one undo step.
13. **A selection in the editor and tables.** A selection in the editor that reaches into a table
    always covers the whole table, all of its lines and the hidden blank line below it: with keys as
    today, with the mouse as well (dragging into a table from above selects to its end, from below
    to its start). A selection of cells that is extended beyond the table (`Shift+Down` in the last
    row, dragging below the table, and the same upwards) becomes such a selection in the editor,
    from the table's start (or end) on. While a selection in the editor covers a table, all its
    cells show the selection colours, without the end cell's frame; this includes `Ctrl+A` in the
    editor and the third `Ctrl+A` of decision 3. What is done with it stays as for text: copy gives
    the Markdown lines (decision 10), cut and delete remove them, typing replaces them, one undo
    step. Parts of a table as text remain possible in the source view only.

## Acceptance scenarios

**Scenario: a rectangle of cells is selected and visible**
- Given a table with a header row and three body rows of four columns, and the caret in the cell of
  row 1, column 1 (counting from 0, the header row being row 0)
- When I drag with the mouse from that cell to the cell of row 3, column 3, or hold `Shift` and
  press `Right` until the cell is selected as a whole, then `Right` twice and `Down` twice
- Then the nine cells of rows 1 to 3, columns 1 to 3 show the selection colours, and no other cell
- And the cell of row 3, column 3 has the end cell's frame
- And the file is unchanged and the document is not marked modified

**Scenario: from text to a cell, with keys and the mouse**
- Given the caret in the middle of the text of a cell with two wrapped lines
- When I press `Shift+Right` until the end of the text
- Then the text from the caret to the end is selected, and no cell
- When I press `Shift+Right` once more
- Then the cell is selected as a cell (selection colours, end cell's frame)
- When I press `Shift+Right` once more
- Then the cell and the one to its right are selected
- And the same holds with `Shift+Down` on the cell's last line (first the end of the text, then the
  cell, then the cell below), with `Shift+Left` and `Shift+Up` towards the start
- When I drag from the cell's text past the end of its text, then into the next cell, then back
  over the text
- Then the selection is the text, then the cell, then both cells, then the text again

**Scenario: selecting rows and columns with keys**
- Given the caret in a body cell
- When I press `Shift+Space`
- Then every cell of that row is selected and shown as selected, and the cell holds no new space
- When I press `Ctrl+Space` (`Control+Space` on macOS)
- Then every cell of that column is selected and shown as selected, the header cell included
- And `Shift` with `Up` or `Down` extends a row selection to adjacent rows, `Shift` with `Left` or
  `Right` a column selection to adjacent columns
- And with a rectangle of cells selected, `Shift+Space` selects all rows of the rectangle and
  `Ctrl+Space` all its columns

**Scenario: selecting rows and columns with the mouse**
- Given a table
- When the mouse pointer is over the area left of a row's `−` control (above a column's), or where
  that control would be for the header row or a single column
- Then the pointer is the arrow pointing right (down), drawn by hitokoto, at the screen's scale
- When I click there, or drag across adjacent areas, or click one area and `Shift`-click another
- Then that row or column, or those rows or columns, are selected and shown as selected
- And the `−` controls still appear and work as before

**Scenario: `Ctrl+A` twice selects the table**
- Given the caret in a cell with text
- When I press `Ctrl+A`
- Then the cell's text is selected
- When I press `Ctrl+A` again
- Then every cell of the table is selected and shown as selected
- And the same happens with one `Ctrl+A` when the cell was selected as a whole with the mouse or
  with keys
- And in an empty cell the first `Ctrl+A` selects the cell as a cell, the second the table
- When I press `Ctrl+A` a third time
- Then the whole document is selected, and the table shows all its cells as selected

**Scenario: the context menu selects**
- Given the context menu of a table cell
- Then it has `Select All`, directly followed by `Select Column` and `Select Row`, and
  `Copy as Tab-Separated Text` directly after `Copy`
- And `Select All`, `Select Column` and `Select Row` select the whole table, the current column or
  the current row
- And a right click inside a selection of cells keeps it

**Scenario: ending a selection of cells**
- Given a rectangle of cells selected with the end cell at row 2, column 2
- When I press `Escape`
- Then no cell is selected and the caret is in the cell of row 2, column 2
- When I press `Escape` again
- Then the caret leaves the table below, as in S018
- And an arrow key without `Shift`, `Tab` or a click end the selection the same way, the click
  with the caret where clicked

**Scenario: copy gives Markdown, and tab-separated text on request**
- Given a table whose cells hold `**bold**`, `a \| b` and `x<br>y`, columns aligned left, centre and
  right
- When I select a rectangle of body cells and press `Ctrl+C`
- Then `text/plain` and `text/markdown` both hold a GFM table: its first line is the first selected
  row, the delimiter row keeps each column's alignment, each cell its Markdown as in the file
- And with the whole table selected, they hold exactly the table's lines; with one cell selected as
  a cell, its text
- When I choose `Copy as Tab-Separated Text`
- Then `text/plain` alone holds the rows with tabs between the cells, `bold`, `a | b` and `"x`
  line break `y"` (quoted, as Excel writes it)
- And pasting that into Excel or Calc puts each cell into a cell of its own (manual check)

**Scenario: hitokoto's own copy has no HTML**
- Given a selection in the editor of a line of text, a blank line and a table
- When I copy it
- Then the clipboard holds `text/plain` and `text/markdown` with exactly the selected lines, and
  the marker, and neither `text/html` nor ODF
- And pasting it into Obsidian gives the text and the table (manual check, Armin's sample of
  2026-11-21)

**Scenario: paste prefers Markdown**
- Given a clipboard with `text/markdown` holding a table and `text/plain` holding other text
- When I press `Ctrl+V` in the editor
- Then the Markdown table is pasted as its own block, as S031 places a table, in one undo step
- When I choose `Paste as Plain Text`
- Then the plain text is pasted
- And copying cells from one table and pasting them into the editor gives a table with the same
  cells, alignment and Markdown

**Scenario: delete, cut, paste and typing act on the selection**
- Given a selection of cells, rows or columns
- When I press `Delete` or `Backspace`
- Then exactly the selected cells are empty; the rows and columns stay
- When I choose `Delete Row` with two whole body rows selected, or `Delete Column` with two whole
  columns selected
- Then exactly those rows or columns are gone
- When I cut
- Then the clipboard holds what copy gives and the cells are empty
- When I paste a table of 2 x 3 cells with the top-left cell of a selection in the last row
- Then the cells from there are filled, a row is added for the rest, and other cells stay
- When I paste one line of text with three cells selected
- Then each of the three cells holds that text
- When I type a character with cells selected
- Then the end cell holds that character only, the other cells keep their text, and no cell is
  selected any more
- And `Ctrl+Z` undoes each of these in one step, and the file holds a valid table, saved byte for
  byte as always (ADR 0002)

**Scenario: a selection in the editor covers whole tables**
- Given text above and below a table
- When I drag from the text above into a cell, or press `Shift+Down` from the line above
- Then the selection in the editor reaches to the end of the table and the table shows all its
  cells as selected, without the end cell's frame
- When I select cells and extend the selection below the table with `Shift+Down` in the last row
- Then the selection in the editor runs from the table's first line to the line below it
- And copying, cutting and deleting act on the selected lines as text, in one undo step

**Scenario: selecting never changes the file** (as in S018)
- When I select cells, rows, columns or the table, with keys, the mouse or the context menu, and
  end the selection again
- Then the file is unchanged and the document is not marked modified

**Scenario: a key the system takes** (manual check on Windows and macOS)
- Given macOS with "Select the previous input source" on `Control+Space`, or Windows with a Chinese
  input method on `Ctrl+Space`
- When I press that key in a cell
- Then the system acts and the column is not selected; `Select Column` in the context menu and the
  column's area still select it
- And the result is recorded in this story; it is no condition for "In review"

**Scenario: selection is themeable** (theming baseline)
- Given a theme that sets every token in the table below
- Then selected cells, the end cell's frame, the current cell, the areas' size and the pointers use
  them
- And a theme that sets none of them falls back as listed, which `docs/theming.md` documents

**Scenario: selecting keeps the budgets**
- Given the 1,000-row table of the benchmarks
- When I select a column, select the table with `Ctrl+A` twice, extend a selection by a row, and
  empty a selected column
- Then each stays within the typing budget (p95 ≤ 16 ms, no keystroke over 50 ms)
- And copying the whole table and pasting a block of cells stay within S031's times for a paste of
  as many cells

## Theming

Every new visible element has its own tokens (brief, section 8; ADR 0004), documented in
`docs/theming.md` with the implementation. The areas for rows and columns are not drawn; their size
is a token. The pointers do not take the theme's colours by default, so that they show on light and
dark tables alike.

| Token | Type | Meaning | Fallback |
|---|---|---|---|
| `table.selection-background` | color | Background of selected cells, and of every cell of a table a selection in the editor covers | `colors.selection` |
| `table.selection-text` | color | Text of selected cells | `colors.selection-text` |
| `table.selection-end-border` | color | Frame of the end cell of a selection of cells | `colors.primary` |
| `table.selection-end-border-width` | number (px) | Width of that frame, 0 to 8 | 2 |
| `table.current-background` | color | Background of the current cell (the cell with the caret) while no cell is selected | `colors.selection` at an alpha of 60 of 255 (as before this story) |
| `table.select-area-size` | number (px) | Width of the area left of a row's `−` control and height of the area above a column's, 4 to 32 | the thickness of the controls (1.3 times the editor font size in points, 14 to 22) |
| `table.select-pointer-fill` | color | Fill of the row and column pointers | `#000000` |
| `table.select-pointer-outline` | color | Outline of the pointers (1 px at 1x) | `#ffffff` |

A selected cell that holds a match of the find bar (S028, "Highlight all") shows the selection
colours while it is selected.

## Not in this story

Several separate rectangles at once, repeating a smaller pasted block over a larger selection,
moving cells by Excel's delayed cut, sorting, merging cells, moving rows or columns by dragging,
formulas, a `text/html` flavour for copies, what cutting the blank line below a table does
(`unrefined.md`).

## Tests

- Core: `S040_TableSelection` for the rules that need no Qt: the rectangle from start and end cell
  and its extension by keys (rows, columns, `Home`/`End`), the GFM table of a selection (header,
  alignment, cell Markdown, the whole table as its lines), the tab-separated text (Excel's quoting,
  shown text), and filling cells from a pasted table (top-left, added rows and columns, header as
  a row), each checked against cmark-gfm where it is Markdown.
- UI: `tst_s040_table_selection` for every scenario, keys only through `test::keyClick` /
  `test::keyClicks` (CLAUDE.md); "shown as selected" checked on the painted cells against the
  theme's colours; the clipboard's formats and their content after each copy; on macOS the column
  key with `Qt::MetaModifier`. The checks marked manual are recorded in this story.
- UI, for the own copy: `tst_s031_paste_tables` (or the S040 test) checks that a copy in the editor
  offers no `text/html` and no ODF and that its `text/markdown` equals its plain text.
- Benchmarks: `s040_*` for selecting a column, `Ctrl+A` twice, extending by a row and emptying a
  column in the 1,000-row table (typing budget), and for copying the table and pasting a block of
  cells (S031's times).
