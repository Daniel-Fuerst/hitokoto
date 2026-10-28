# ADR 0005: Interactive Markdown tables

- **Status:** Accepted 2026-10-26; revised and accepted 2026-10-27 after the code review of S018
  and ADR 0003
- **Deciders:** project owner

## Context

The brief (section 1) defines a hybrid editor: the document is plain Markdown, rendered inline,
with syntax shown where the caret is. For GitHub Flavored Markdown pipe tables, showing pipes and
delimiter rows while editing makes the writer manage column counts and alignment rows by hand.
Editors such as Obsidian show tables as a grid of editable cells instead.

The first implementation (2026-10-26) laid out each cell as its own `QTextLine` inside a
`QPlainTextEdit` block and rebuilt navigation, deletion and hit testing on top. The code review
found that this fights Qt's text layout (scroll loop O2, design O8), lets typed-over selections
and paste break rows (O4, O5), changes the file while navigating (O6), hides text for good (O7)
and deviates from GFM for pipes in code spans (O1). Cells also need line breaks (`<br>`), which
we do not want to render ourselves. ADR 0003 gave the editor a layout in which a line can
reserve space for an embedded widget.

## Decision

1. **A table is an embedded Qt table view.** The header line of a table reserves the height of a
   `QTableView` in `BlockLayout` (ADR 0003); the delimiter and body lines are hidden. The view is
   placed over the reserved space and scrolls with the text. Views exist only for tables on or
   near the screen.
2. **The Markdown lines stay the only copy (ADR 0002).** A table model (`QAbstractTableModel`)
   reads cells from the lines through `hitokoto_core` and writes a changed cell back into its
   span of its line with `QTextCursor`, in one edit block, so undo stays in the document. Moving,
   selecting and scrolling never write. Writing escapes `|` as `\|` and line breaks as `<br>`;
   rows with fewer cells are padded only when a missing cell is written.
3. **GFM rules, checked against cmark-gfm.** Every unescaped `|` separates cells (also inside code
   spans); a line without a pipe continues the table; the table ends at a blank line or another
   block start; excess cells are not part of the table; alignment holds for every column.
4. **Qt draws and edits the cells.** A delegate draws each cell with a `QTextDocument` filled by
   `setMarkdown` (GitHub dialect), after replacing `<br>` with `<br/>`: Qt 6.12 drops the rest of
   the input after a `<br>` without slash. Wrapping, row heights and column widths come from the
   view. The cell editor is a Qt text editor showing the cell's Markdown with real line breaks;
   `Shift+Enter` inserts one.
5. **Source view.** A shortcut and a context-menu entry switch one table between the view and its
   Markdown lines (`BlockLayout` shows the lines and releases the reserved height). Rows with text
   the view does not show are marked in the view.
6. **Theme tokens** stay as decided on 2026-10-26:

| Token | Type | Fallback |
|---|---|---|
| `table.background` | color | `colors.background` |
| `table.header-background` | color | `colors.surface` |
| `table.stripe-background` | color | `table.background` |
| `table.text` | color | `colors.text` |
| `table.header-text` | color | `table.text` |
| `table.header-weight` | number, 100 to 900 | `headings.weight` |
| `table.border` | color | `colors.border` |
| `table.border-width` | number (px), 0 to 8 | 1 |
| `table.radius` | number (px), 0 to 32 | `layout.radius` |
| `table.padding` | number (px), 0 to 64 | `layout.spacing` |
| `table.font-family` | string | `""` = editor font |
| `table.font-scale` | number, 0.5 to 2 | 1 |

## Consequences

- Caret, selection, input methods and accessibility inside cells come from Qt's table view and
  text editor; S009 checks them in a table scenario.
- The cell-line layout, `TableController` and `TablePainter` of the first implementation go;
  `hitokoto_core` keeps parsing and the row and column helpers.
- A selection in the editor that spans a table copies its Markdown lines.
- Saving writes the lines as they are: untouched tables and untouched cells stay byte for byte.

## Reconsider if

- Users need block content in cells (lists, code blocks), which GFM tables do not support.
- Embedded views cost too much for documents with many large tables; then draw inactive tables
  and create a view only for the table being edited.
