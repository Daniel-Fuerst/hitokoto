---
mode: code
spec: docs/stories/041-table-copy-paste.md
commits: fc21d63..f42538f (branch agents/s041-table-copy-paste)
reviewer: Claude (Advocatus Diaboli), 2026-11-26, night mode
objections:
  - id: O1
    category: failure
    severity: major
    title: One spreadsheet row or column pasted into cells lands as one text with tabs in every cell
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: failure
    severity: minor
    title: An HTML table with any text beside it fills every selected cell with the whole text
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: minor
    title: A tab-separated field that starts with a quote is written unquoted
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: failure
    severity: minor
    title: One cell copied as a cell puts its text, not its Markdown, into text/markdown
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: operational
    severity: minor
    title: The Edit menu's Cut, Copy and Paste ignore the selected cells and the cell with the caret
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: operational
    severity: minor
    title: Cut, Copy and Paste of the cell's context menu are untested
    disposition: pending
    disposition_rationale: null
  - id: O7
    category: cost
    severity: minor
    title: Pasting hitokoto's own table in the editor has no benchmark and no wait cursor
    disposition: pending
    disposition_rationale: null
---

# S041 — Copying and pasting table cells as Markdown: objection record (code)

Examined: the eight commits from `fc21d63` (the S040 branch with the gate fix) to `f42538f`: core
`table_clipboard.hpp`/`.cpp` and `S041_TableCopyPaste`, `EditorWidget` (own copy, paste of the own
Markdown), `TableView`/`CellEditor` (keys, context menu, Paste as Plain Text), `TableEmbedder`
(`copyCells`, `pasteIntoCells`, `replaceLines`), `table_paste` (`pastedCells`), `main_window`,
`tst_s041_table_copy_paste`, the changed `tst_s031_paste_tables`, the benchmark and the gate.

## O1 - failure - major

**One spreadsheet row or column pasted into cells lands as one text with tabs in every cell.**
`pastedCells` (`src/ui/table_paste.cpp`) takes Excel's text only if `core::countsAsTable` holds,
which needs at least two rows and two columns (`src/core/src/paste.cpp`, `countsAsTable`). Copying
one row from Excel (`a\tb\tc\r\n`) or one column is therefore "text that is no table": with three
cells selected, `pasteTextIntoTableCells` writes `a<tab>b<tab>c` into each of them
(`tableCellFromText` keeps the tab); with only the caret, the whole row goes into one cell, as
before. Filling a row of a table from a spreadsheet row is a common use of decision 7, and the
result is three cells holding the same tab-separated text. The story ties the table to "S031's
rules", which were written for the editor, where a single row should stay text; in cells the
reason for that rule does not hold. The same applies to an HTML table of one row.

## O2 - failure - minor

**An HTML table with any text beside it fills every selected cell with the whole text.**
`htmlTableCells` (`src/ui/table_paste.cpp`) gives up when any part outside the table holds a
non-blank line, so a copy from Word or a web page that carries a caption or a heading with the
table becomes text: with cells selected, every one of them gets the paste's entire plain text, its
rows joined by `<br>`. S031 converts the same paste in the editor (table as a table, text as text).
Decision 7 does not say what a table with text beside it does in cells; the implementation chose
the most surprising of the readings.

## O3 - failure - minor

**A tab-separated field that starts with a quote is written unquoted.** `appendField`
(`src/core/src/table_clipboard.cpp`) quotes only for a tab or a line break, as decision 4 says.
A spreadsheet reads a `"` at the start of a field as the start of a quoted field, so a cell
`"Bio" Tee` arrives as `Bio Tee` or with the quotes moved. hitokoto's own reader
(`parseExcelPlainText`) keeps the characters, so the round trip within hitokoto does not show it;
`TabSeparatedTextQuotesAsExcelDoes` only tests a quote inside a field. To be confirmed in the
manual check with Excel and Calc.

## O4 - failure - minor

**One cell copied as a cell puts its text, not its Markdown, into `text/markdown`.** For one cell,
`copiedTableCells` returns `tableCellToText` (`\|` as `|`, `<br>` as a line break), as decision 2
asks ("as copying its whole text does today"), and the embedder puts it into both formats with the
marker (`TableEmbedder::copyCells`). Decision 1 calls `text/markdown` "exactly the selected text of
the file"; for a cell `a \| b` it holds `a | b`, and pasted into a table's lines in source mode
(or into another Markdown editor) it splits the cell. Two decisions of the story disagree here.

## O5 - operational - minor

**The Edit menu's Cut, Copy and Paste ignore the selected cells and the cell with the caret.**
`src/ui/main_window.cpp` connects them to the editor (`editor_, &QTextEdit::cut` and so on). The
keys reach the cell first, so `Ctrl+C` works, but choosing Edit → Copy with cells selected copies
the editor's selection, and Edit → Paste with the caret in a cell pastes at the editor's hidden
cursor. The second was so before S041; S041 makes the menu and the keys differ for the new
actions. Documented in the story as not covered; the menu is the place users look for them.

## O6 - operational - minor

**Cut, Copy and Paste of the cell's context menu are untested.**
`CellEditor::cellClipboardEntries` (`src/ui/table_view.cpp`) disconnects the standard actions and
connects them to the view while cells are selected. `tst_s041_table_copy_paste` only checks the
menu's entries (`copyingAsTabSeparatedText`) and triggers the keys and the view's action; no test
triggers the menu's Copy, Cut or Paste with cells selected. A change of the standard actions'
object names in Qt (`edit-copy`, `edit-cut`, `edit-paste`) would leave them on the cell's text
without a failing test.

## O7 - cost - minor

**Pasting hitokoto's own table in the editor has no benchmark and no wait cursor.**
`EditorWidget::pasteOwnTable` reads the Markdown with `markdownTableCells` (the registry renders
every line) and places it as S031 places a table, but the wait cursor of S031 is not shown and the
benchmarks only measure pastes into cells (`s041_paste_*`). The scenario "copying and pasting keep
the budgets" names a paste of cells only, so the budget is met as written; a 1,000-row table
pasted below text has no number.

## Explicitly not objecting to

- The eager own copy in the editor: `createMimeDataFromSelection` now builds the text at once
  instead of the lazy formats of S031 review (code) O9. Measured with Qt 6.12.0 (release, cloud VM)
  on a 10 MB document, select all: 157 to 205 ms for the text, its UTF-8 and the data, against
  1,660 to 1,800 ms for the `QTextDocumentFragment` the old copy built at once. Faster than before;
  still above 50 ms for 10 MB, as it was.
- The marker being trusted: any program can set `application/x-hitokoto-copy`, and hitokoto then
  pastes the Markdown instead of the plain text. Decision 5 asks for exactly this; the data still
  goes through `insertableText`, the 255-column limit and the normal placement.
- `replaceLines` adding rows after the table's last line: one edit block, so one undo step, tested
  by `pastingATableAddsARowAndKeepsTheOtherCells` and by mutation.
- The line endings: `text/markdown` is built from LF text on every platform; the test reads it
  back through the real clipboard on Windows and macOS CI, in process on Linux.
- Paste as Plain Text in a cell staying text at the caret (`CellEditor::pasteAsPlainText`): it keeps
  the decision of S031 review O2 and decision 5's "still takes only `text/plain`".
