# S040 — Selecting cells, rows and columns in a table

- **Status:** In progress (confirmed by Armin on 2026-11-21, after the review
  `docs/reviews/040-table-selection-spec.md` and its dispositions) · **Order:** 46 · **Size:** L ·
  **Depends on:** S018, S019, S028, S031
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
  - The height of a table view after cutting or deleting some of its lines was checked as well (a
    point found in the rework of S028 (2026-11-15)): it equals the height in a fresh window with the
    same text, also right after the change, after undo and after redo. Where it differed, the cut
    had taken the blank line below the table, so the line after it became a row (GFM); a fresh
    window adds a blank line there when the file is opened (S018, "files from other editors").
    That is a different point and is in `unrefined.md` ("Cutting the blank line below a table").
- **Split:** copying, cutting and pasting selected cells, and hitokoto's clipboard formats, are
  S041, which builds on this story (Armin, 2026-11-21, after the review of this story).

## Story

As a writer, I want to select any rectangle of cells, whole rows, whole columns or the whole table
the way I know it from Word and spreadsheets, and see at once which cells are selected, so that I
can act on exactly what I see.

## Decisions (Armin, 2026-11-21)

1. **Any rectangle of cells.** A selection of cells is a rectangle: in a 4 x 4 table any 2 x 2 or
   3 x 2 block, adjacent rows together, adjacent columns together, or the whole table. It grows and
   shrinks from the cell where it started, as a text selection does from its anchor; no cell is
   marked as the active one (as in Word).
2. **Selected cells are visible as selected.** A cell selected as a cell shows the selection
   colours over its whole area. Text selected inside a cell shows as today, the text alone, also
   when it is all of the cell's text.
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
   (Armin: a habit, as in spreadsheets); decision 11 keeps a `Shift` held too long while typing from
   changing a cell. While an input method composes text (pre-edit), these keys belong to it. Note
   for the implementation: Qt maps `Qt::ControlModifier` to Command on macOS; the column key there
   needs `Qt::MetaModifier` (the Control key), and `Ctrl+A` goes through `QKeySequence::SelectAll`.
5. **When the system takes a key.** Where the system or an input method uses `Ctrl+Space`,
   `Control+Space` or `Shift+Space` itself, the key never reaches hitokoto, and hitokoto offers no
   other key for it (Excel has the same overlap). Known cases: macOS selects the previous input
   source with `Control+Space` by default; the Chinese input methods of Windows switch with
   `Ctrl+Space`; fcitx on Linux switches with `Ctrl+Space` and, with a Chinese input method on,
   toggles full and half width with `Shift+Space`. Rows and columns stay selectable with the mouse
   (decision 6) and the context menu (decision 7).
6. **Mouse on rows and columns.** Between a row's `−` control (the control that deletes it) and the
   table lies the row's area, between a column's `−` control and the table the column's area: from
   the first pixel after the control, towards the table, up to and including the pixel of the
   table's border, so today the gap of 2 px plus `table.border-width`. The header row and a table's
   only column, which have no `−` control, have their area in the same place. The space around the
   table and its layout do not change. A click in an area selects the row or column, dragging across
   adjacent areas selects adjacent rows or columns, and `Shift` with a click extends from where the
   selection started. The area is not drawn: over it, the mouse pointer shows it, an arrow pointing
   right over a row's area and an arrow pointing down over a column's area. The `−` controls stay
   where they are and appear as before. If the area proves too small in practice, it is widened
   later; until then the context menu selects rows and columns as well.
   The two pointers are drawn for hitokoto (no third-party images): a filled arrow with a straight
   shaft and a triangular head, the shaft about half as high as the head and the head about as
   long as the shaft; the down arrow is the same shape turned by 90 degrees. Details (adjustable
   later): black fill with a white outline of 1 px at 1x, as the system's own pointers, so that
   they show on light and dark tables; 32 x 32 px at 1x with versions for 2x, drawn as SVG in the
   repository and turned into a `QCursor` at the screen's scale through Qt SVG, which hitokoto
   links already (no image format plugin, S037); hot spot at the tip of the arrow.
7. **Context menu of the table.** `Select All` selects the whole table (matching its icon).
   Directly below it: `Select Column` and `Select Row`. A right click inside a selection of cells
   keeps it; a right click on another cell ends it, and that cell becomes the current one.
8. **From text to cells.** A selection starts as text in the current cell and becomes a selection
   of cells at the edge of the cell's text:
   - `Shift+Right` selects up to the end of the text; the next `Shift+Right` selects the whole cell
     as a cell, wherever the text selection started (also when the whole text was selected); the
     next one adds the cell to the right, and from then on the selection grows and shrinks by whole
     cells, never again by parts of a cell's text. `Shift+Left` in the same way towards the start.
   - `Shift+Down` moves line by line through the cell's wrapped text; on its last line it selects
     up to the end of the text, as today; the next `Shift+Down` selects the whole cell; the next one
     adds the cell below. `Shift+Up` in the same way towards the start.
   - With cells selected, `Shift+Home` and `Shift+End` extend to the first or last column,
     `Ctrl+Shift+Home` and `Ctrl+Shift+End` to the first or last cell of the table. With text
     selected they act on the text, as today.
   - Dragging with the mouse selects text while the pointer stays over the text of the cell where
     the drag started; past the end of the cell's text it selects the whole cell; in another cell
     it selects the rectangle from the starting cell to that cell; back over the text of the
     starting cell it is a text selection again. `Shift` with a click on another cell selects the
     rectangle from the current cell to that cell.
9. **Ending a selection of cells.** Each of these ends it:
   - `Escape`; the caret is where it was before the selection began. A second `Escape` leaves the
     table, as in S018.
   - An arrow key without `Shift`: the caret moves as if there had been no selection, from where
     it was before the selection began.
   - `Tab` and `Shift+Tab`: to the next or previous cell, counted from that same place.
   - A click: the caret goes where the click was.
   - `Ctrl+Shift+M`: the table shows its source with the caret on the line of the cell where the
     caret was before the selection began.
   - A change of the table's lines that does not come from the selection: undo, redo, the add and
     delete controls of S018, and a table that is rebuilt because a change touched its lines (S018,
     "Robustness"). The caret then goes where S018 puts it after such a change.
   A text selection inside one cell behaves as today.
10. **Deleting.**
    - `Delete` and `Backspace` empty the selected cells; the rows and columns stay. With the whole
      table selected they remove the table: all its lines and the hidden blank line below it, so
      the text before and after it stays apart by the blank line above the table.
    - `Delete Row` and `Delete Column` (context menu and the keys of S018) remove all selected whole
      rows or columns at once. The header row stays, as in S018: with only the header row selected,
      `Delete Row` is disabled in the menu and its keys do nothing; with the whole table selected it
      removes every body row. With every column selected, `Delete Column` removes all but the first
      (Armin: to be tried in practice).
    - Each of these is one undo step, and the file holds a valid table (or none), saved byte for
      byte as always (ADR 0002).
11. **Typing with cells selected.** A typed character, a paste of text and committed input method
    text replace nothing: the selection of cells ends, and the text goes into the cell where the
    caret was before the selection began, at that place. So a `Shift` held a moment too long while
    typing (`Shift+Space` selects the row) costs a space, not the text of a cell. To replace cells,
    delete them first (decision 10). What a paste does with a table on the clipboard is S041.
12. **A selection in the editor and tables.** A selection in the editor that reaches into a table
    always covers the whole table, all of its lines and the hidden blank line below it: with keys as
    today, with the mouse as well (dragging into a table from above selects to its end, from below
    to its start). A selection of cells that is extended beyond the table (`Shift+Down` in the last
    row, dragging below the table, and the same upwards) becomes such a selection in the editor,
    from the table's start (or end) on. While a selection in the editor covers a table, all its
    cells show the selection colours; this includes `Ctrl+A` in the editor and the third `Ctrl+A`
    of decision 3. What is done with it stays as for text: cut and delete remove the lines, typing
    replaces them, one undo step; what copy puts on the clipboard is S041. Parts of a table as text
    remain possible in the source view only.
13. **Screen readers.** The selection of cells is held in the table view's selection model, so
    that Qt's accessible table interface reports the selected cells, rows and columns. S009 gains a
    scenario for it.

## Implementation (2026-11-26, night mode; in progress)

- Core: `core::TableCellSelection` (`table_selection.hpp`) holds the rectangle from its anchor,
  whole rows and columns, and the steps of decision 8; `shiftStepSelectsCell` the step from text
  to cell; `emptyTableCells`, `removeTableRows` and `removeTableColumns` the lines of decision 10.
- The table view selects nothing by itself (`NoSelection`); a selection of cells is set in its
  selection model, which the accessible table reports (decision 13, S009 scenario). The open editor
  of the current cell, which covers its cell, takes the selection colours through a style sheet of
  its own while cells are selected or the editor's selection covers the table: the view's style
  sheet reaches it, so its palette alone does not show.
- The areas of rows and columns (decision 6) are found by `TableWidget::areaAt`; the pointers are
  `icons/select-row.svg` and `icons/select-column.svg`, rendered in the theme's colours.
- A selection in the editor that ends inside a table's lines is widened to the table's end (from
  above, the hidden blank line below it included) or start (from below) on every selection change,
  so keys and the mouse behave alike.

- Benchmarks: `s040_selection_*` in hitokoto-bench measure every key of a column, `Ctrl+A` twice,
  a row extended by one, an emptied column and the removed table, ten rounds in the 1,000-row
  table; `s040_selection_max_ms` is a hard budget of the gate. Release build in the cloud VM
  (2 vCPU, Qt 6.12.0, offscreen), 2026-11-26: p95 9.75 ms, highest 14.4 ms, none over 16 ms.

Open before "In review": dragging with the mouse from a selection of cells to below or above the table (the view keeps the mouse
while the button is held, so the drag stops at the edge rows; Shift with the arrows does extend it
into the editor); the manual checks (a key the system takes, the pointers on Windows and macOS at
2x).

## Acceptance scenarios

**Scenario: a rectangle of cells is selected and visible**
- Given a table with a header row and three body rows of four columns, and the caret in the cell of
  row 1, column 1 (counting from 0, the header row being row 0)
- When I drag with the mouse from that cell to the cell of row 3, column 3, or hold `Shift` and
  press `Right` until the cell is selected as a whole, then `Right` twice and `Down` twice
- Then the nine cells of rows 1 to 3, columns 1 to 3 show the selection colours over their whole
  area, and no other cell does
- And the file is unchanged and the document is not marked modified

**Scenario: from text to a cell, with keys and the mouse**
- Given the caret in the middle of the text of a cell with two wrapped lines
- When I press `Shift+Right` until the end of the text
- Then the text from the caret to the end is selected and shown as text, and no cell
- When I press `Shift+Right` once more
- Then the cell is selected as a cell and shows the selection colours over its whole area
- When I press `Shift+Right` once more
- Then the cell and the one to its right are selected, and further presses add or remove whole
  cells only
- And the same holds with `Shift+Down` on the cell's last line (first the end of the text, then the
  cell, then the cell below), and towards the start with `Shift+Left` and `Shift+Up`
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
- When the mouse pointer is between a row's `−` control and the table, the table's border included
  (between a column's `−` control and the table), or in that place for the header row or a single
  column
- Then the pointer is the arrow pointing right (down), drawn by hitokoto, at the screen's scale
- When I click there, or drag across adjacent areas, or click one area and `Shift`-click another
- Then that row or column, or those rows or columns, are selected and shown as selected
- And the `−` controls still appear and work as before, and the table's position and size are as
  before this story

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
- Then it has `Select All`, directly followed by `Select Column` and `Select Row`
- And they select the whole table, the current column or the current row
- And a right click inside a selection of cells keeps it

**Scenario: ending a selection of cells**
- Given the caret in the middle of the text of the cell of row 1, column 1, and a rectangle of
  cells selected from there to row 2, column 2
- When I press `Escape`
- Then no cell is selected and the caret is where it was before the selection, in row 1, column 1
- When I press `Escape` again
- Then the caret leaves the table below, as in S018
- When I select the rectangle again and press `Right`
- Then no cell is selected and the caret is one character to the right of where it was before the
  selection
- And `Tab`, `Shift+Tab`, a click and `Ctrl+Shift+M` end it as decision 9 says

**Scenario: typing never replaces selected cells**
- Given text being typed in a cell and `Shift` still held when `Space` is pressed, so that the row
  is selected
- When I type the next character
- Then no cell is selected, the character follows the text typed before at the caret, and every
  other cell of the row keeps its text
- And the same holds for a paste of text and for text committed by an input method

**Scenario: deleting acts on the selection**
- Given a selection of cells, rows or columns, not the whole table
- When I press `Delete` or `Backspace`
- Then exactly the selected cells are empty; the rows and columns stay
- When I choose `Delete Row` with two whole body rows selected, or `Delete Column` with two whole
  columns selected
- Then exactly those rows or columns are gone
- When I select only the header row
- Then `Delete Row` is disabled, and its keys change nothing
- When I select every column and choose `Delete Column`
- Then only the first column remains
- When I select the whole table and press `Delete` or `Backspace`
- Then the table's lines and the blank line below it are gone, and the text before and after it is
  separated by one blank line
- And `Ctrl+Z` undoes each of these in one step, and the file is saved byte for byte as always

**Scenario: a selection in the editor covers whole tables**
- Given text above and below a table
- When I drag from the text above into a cell, or press `Shift+Down` from the line above
- Then the selection in the editor reaches to the end of the table and the table shows all its
  cells as selected
- When I select cells and extend the selection below the table with `Shift+Down` in the last row
- Then the selection in the editor runs from the table's first line to the line below it
- And cutting and deleting act on the selected lines as text, in one undo step

**Scenario: a screen reader learns the selection** (S009)
- Given a selection of two cells, a row or a column
- Then the table view's accessible interface reports exactly those cells as selected, and the
  selected rows or columns

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
- Then selected cells, the current cell and the pointers use them
- And a theme that sets none of them falls back as listed, which `docs/theming.md` documents

**Scenario: selecting keeps the budgets**
- Given the 1,000-row table of the benchmarks
- When I select a column, select the table with `Ctrl+A` twice, extend a selection by a row, empty a
  selected column and remove the selected table
- Then each stays within the typing budget (p95 ≤ 16 ms, no keystroke over 50 ms)

## Theming

Every new visible element has its own tokens (brief, section 8; ADR 0004), documented in
`docs/theming.md` with the implementation. The areas for rows and columns are not drawn and take no
space of their own. The pointers do not take the theme's colours by default, so that they show on
light and dark tables alike.

| Token | Type | Meaning | Fallback |
|---|---|---|---|
| `table.selection-background` | color | Background of cells selected as cells, and of every cell of a table a selection in the editor covers | `colors.selection` |
| `table.selection-text` | color | Text of those cells | `colors.selection-text` |
| `table.current-background` | color | Background of the current cell (the cell with the caret) while no cell is selected | `colors.selection` at an alpha of 60 of 255 (as before this story) |
| `table.select-pointer-fill` | color | Fill of the row and column pointers | `#000000` |
| `table.select-pointer-outline` | color | Outline of the pointers (1 px at 1x) | `#ffffff` |

A selected cell that holds a match of the find bar (S028, "Highlight all") shows the selection
colours while it is selected.

## Not in this story

Copying, cutting and pasting selected cells and hitokoto's clipboard formats (S041); several
separate rectangles at once; sorting, merging cells, moving rows or columns by dragging, formulas;
a wider area for rows and columns (only if practice asks for it); what cutting the blank line below
a table does (`unrefined.md`).

## Tests

- Core: `S040_TableSelection` for the rules that need no Qt: the rectangle from where it started
  and its extension by keys (cells, rows, columns, `Home`/`End`), the step from text to cell at the
  edge of a cell's text, and the lines that deleting cells, rows, columns or the whole table
  produces.
- UI: `tst_s040_table_selection` for every scenario, keys only through `test::keyClick` /
  `test::keyClicks` (CLAUDE.md); "shown as selected" checked on the painted cells against the
  theme's colours; the pointer's shape over the areas and the unchanged geometry of the table; on
  macOS the column key with `Qt::MetaModifier`; the accessible interface's selected cells. The
  checks marked manual are recorded in this story.
- S009: `tst_s009_concealment` gains the screen reader case of a selection of cells.
- Benchmarks: `s040_*` for selecting a column, `Ctrl+A` twice, extending by a row, emptying a
  column and removing the table in the 1,000-row table (typing budget).
