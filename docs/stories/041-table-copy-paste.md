# S041 — Copying and pasting table cells as Markdown

- **Status:** Confirmed (Armin, 2026-11-21, after the review of S040, from which it was split) ·
  **Order:** 47 · **Size:** M · **Depends on:** S031, S040
- **Basis:** split from S040 (Armin, 2026-11-21, after the review of S040): what copy, cut and
  paste do with selected cells. Checked on main `c8a167f` and with Armin's clipboard samples on
  Windows (2026-11-21), before this story was written:
  - Copying in the editor offers Qt's formats for rich text: `text/plain`, `text/html`,
    `text/markdown` and ODF (`EditorWidget::createMimeDataFromSelection`, made on demand since
    S031), and the marker `application/x-hitokoto-copy`. The plain text is exactly the selected
    lines of the file.
  - Obsidian takes the HTML when it is there and makes each line a paragraph of its own, so a
    copied table with text before it arrives as loose lines; a copy of the table alone arrived as a
    table. Pasted as plain text (`Ctrl+Shift+V` in Obsidian), both arrive as a table.
  - Qt's `text/markdown` (`QTextDocument::toMarkdown`) puts a blank line between the table's lines,
    so it is no table either. Any Qt program with a rich-text editor offers the same.
  - Obsidian itself puts only HTML and `text/plain` on the clipboard; its plain text is the
    Markdown, without blank lines between a table's lines (Armin).
  - On Windows the plain text on the clipboard has CRLF line endings; the `text/markdown` stream of
    the same copy has LF.

Armin's sample (2026-11-21) is a file of these lines; the copy with `Text1` and `Text2` selected fell
apart in Obsidian, the copy of the table alone arrived as a table:

```markdown
Text1

| Linke Spalte | Rechte Spalte |
| --- | --- |
| Wert 1 | Wert 2 |


Text2
```

## Story

As a writer, I want selected cells to go to the clipboard as Markdown, so that they arrive as a
table in hitokoto, Obsidian and on GitHub, and on request as tab-separated text for a spreadsheet;
and I want to paste a table into cells.

## Decisions (Armin, 2026-11-21)

1. **hitokoto's own copy is Markdown only.** In the editor and from a cell's text: `text/plain` and
   `text/markdown`, both holding exactly the selected text of the file, and the marker
   `application/x-hitokoto-copy` of S031. No `text/html` and no ODF any more (they were Qt's
   rendering of plain paragraphs; hitokoto writes no HTML, brief section 2).
2. **Copying selected cells (S040).** `Ctrl+C` puts the cells as a GFM table into `text/plain` and
   `text/markdown`, in S031's minimal form (`| a | b |`, delimiter `| --- |`, no padding). The first
   selected row is the header row; each column keeps its alignment from the delimiter row; each cell
   holds its Markdown as in the file (`**bold**`, `\|`, `<br>` unchanged). The whole table gives
   exactly its lines, without the blank line below it. A single cell selected as a cell gives its
   text, as copying its whole text does today. A selection in the editor that covers tables (S040,
   decision 12) gives its lines as text, as decision 1 says.
3. **Line endings.** `text/markdown` holds LF, checked by a test on Linux, Windows and macOS; should a
   platform convert it, its own convention applies there. `text/plain` follows the platform's
   convention, as Qt writes it (CRLF on Windows).
4. **`Copy as Tab-Separated Text`.** A context menu entry directly below `Copy`, enabled with cells
   selected. It puts only `text/plain`, tab-separated in Excel's form, for spreadsheets: rows end
   with a line break, cells are separated by a tab, a field is quoted only when it holds a tab or a
   line break, and `"` inside it is doubled. Each field is the text the cell shows, as the find bar
   searches it (S028, "Table cells"): the inline elements concealed (a link's text without its
   destination, code without its backticks, escapes resolved), `\|` as `|`, `<br>` as a line break,
   entities as written.
5. **Paste prefers hitokoto's own Markdown.** When the clipboard holds `text/markdown` and the
   marker `application/x-hitokoto-copy`, `Ctrl+V` in the editor pastes that Markdown as it is
   written; the clipboard's other formats are not looked at. If it is exactly one GFM table, it
   lands as S031 places a table ("Where it lands": its own block, one undo step); any other
   Markdown is inserted as text. Without the marker, a paste works as today (S031): other programs'
   `text/markdown` can differ from their text. `Paste as Plain Text` still takes only `text/plain`.
   S031 and ADR 0011 get a dated note with the implementation.
6. **Cut.** `Cut` copies as in decisions 2 to 4 and empties the selected cells at once, as Calc
   does; with the whole table selected it removes the table as `Delete` does (S040, decision 10).
7. **Pasting into cells.** With cells selected, or with the caret in a cell:
   - A table on the clipboard (hitokoto's own `text/markdown` holding a table, or an HTML or
     tab-separated table under S031's rules) fills the cells from the top-left selected cell, or
     from the cell with the caret. Its header row is an ordinary row there. Rows and columns are
     added as needed, as adding does in S018; cells outside the pasted block stay. Cells from
     `text/markdown` keep their Markdown; cells from HTML or tab-separated text follow S031's rules
     for cell text and escaping.
   - A paste that would need more than 255 columns (S018) changes nothing.
   - Text that is no table replaces the content of every selected cell, with its line breaks as
     `<br>` and `|` as `\|`, as in S018. With the caret in a cell and no cell selected, it is
     pasted at the caret as today.
   - Each paste is one undo step, and the file is saved byte for byte as always (ADR 0002).

## Acceptance scenarios

**Scenario: copying cells gives a Markdown table**
- Given a table whose cells hold `**bold**`, `a \| b` and `x<br>y`, columns aligned left, centre and
  right
- When I select a rectangle of body cells and press `Ctrl+C`
- Then `text/plain` and `text/markdown` both hold a GFM table: its first line is the first selected
  row, the delimiter row keeps each column's alignment, each cell its Markdown as in the file
- And with the whole table selected, they hold exactly the table's lines; with one cell selected as
  a cell, its text
- And `text/markdown` has LF line endings on every platform

**Scenario: copying as tab-separated text**
- Given the same table and a selection of cells
- When I choose `Copy as Tab-Separated Text`
- Then `text/plain` alone holds the rows with tabs between the cells, `bold`, `a | b` and `"x`
  line break `y"` (quoted, as Excel writes it)
- And pasting that into Excel or Calc puts each cell into a cell of its own (manual check)

**Scenario: hitokoto's own copy has no HTML**
- Given a selection in the editor of a line of text, a blank line and a table
- When I copy it
- Then the clipboard holds `text/plain` and `text/markdown` with exactly the selected lines, and
  the marker, and neither `text/html` nor ODF
- And pasting it into Obsidian gives the text and the table (manual check with Armin's sample in
  the basis, `Text1` to `Text2` selected)

**Scenario: paste prefers hitokoto's own Markdown**
- Given cells copied in hitokoto
- When I press `Ctrl+V` in the editor
- Then a table with the same cells, alignment and Markdown is pasted as its own block, in one undo
  step
- Given a clipboard from another program with `text/markdown` that differs from its `text/plain`
- When I press `Ctrl+V` in the editor
- Then the paste is as before this story (S031)

**Scenario: cut and paste act on cells**
- Given a selection of cells
- When I cut
- Then the clipboard holds what copy gives and the cells are empty
- When I paste a table of 2 x 3 cells with the top-left cell of a selection in the last row
- Then the cells from there are filled, a row is added for the rest, and other cells stay
- When I paste a table that would need more than 255 columns
- Then nothing changes
- When I paste one line of text with three cells selected
- Then each of the three cells holds that text
- And `Ctrl+Z` undoes each of these in one step

**Scenario: copying and pasting keep the budgets**
- Given the 1,000-row table of the benchmarks
- When I copy the whole table, copy it as tab-separated text, and paste a block of cells
- Then the copy stays within the typing budget, and the paste within S031's times for a paste of as
  many cells

## Theming

No new visible element: the context menu entry uses the menu's tokens.

## Not in this story

Repeating a smaller pasted block over a larger selection; Excel's delayed cut; a `text/html` flavour
for copies; preferring other programs' Markdown on paste.

## Tests

- Core: `S041_TableCopyPaste` for the GFM table of a selection (header, alignment, cell Markdown,
  the whole table as its lines), the tab-separated text (Excel's quoting, the cell text of S028),
  and filling cells from a pasted table (top-left, added rows and columns, header as a row, the
  limit of 255 columns), each checked against cmark-gfm where it is Markdown.
- UI: `tst_s041_table_copy_paste` for every scenario: the clipboard's formats and their content
  after each copy, the marker, line endings per platform, keys only through `test::keyClick` /
  `test::keyClicks`; `tst_s031_paste_tables` keeps passing, extended where the own copy changed.
- Benchmarks: `s041_*` for copying the 1,000-row table both ways and pasting a block of cells.
