# S031 — Pasting a table from Outlook, Excel or a web page as a Markdown table

- **Status:** In progress (red tests 2026-11-09); confirmed by Armin, 2026-11-09, after review
  `docs/reviews/031-paste-tables-spec.md` ·
  **Order:** 32 · **Size:** M · **Depends on:** S018, S019, S020 · **Decision:** ADR 0011
- **Basis:** Armin's feature request and decisions of 2026-11-09. A table copied in Outlook and
  pasted into Obsidian becomes a Markdown table; in hitokoto it arrives as tab-separated plain text,
  and cells with several lines fall apart. Pasting with conversion is not in the brief; this story is
  the scope extension that brings it in (CLAUDE.md: scope changes become new stories). Reading the
  clipboard's HTML touches the brief, section 2 ("No HTML"); ADR 0011 records the decision.

## Story

As a writer, I want a table I copy in Outlook, Excel, Word or a web browser to arrive as a Markdown
table when I paste it, with each cell's lines kept in its cell, so that I do not have to rebuild the
table by hand.

## Decisions (Armin, 2026-11-09)

1. **The HTML flavour of the clipboard is read, to recognise tables only** (ADR 0011). Its text and
   structure are taken; nothing is shown or loaded as HTML.
2. **Fallback: Excel's plain text.** Without an HTML flavour, tab-separated plain text in Excel's
   form becomes a table too, under the rules below. Excel puts only a cell with a line break or a
   tab in double quotes and doubles each `"` inside it; a `"` in any other cell stays as it is
   (Excel samples, Armin, 2026-11-09).
3. **Minimal Markdown:** `| a | b |`, delimiter row `| --- | --- |`, no padding to column widths,
   no alignment. A line break inside a cell is written as `<br>` (as `Shift+Enter` in S018), without
   the two spaces Obsidian puts before it.
4. **Text around the tables is kept as plain text** (review O1, after an Outlook sample with text
   before and after the table): the tables are converted, the text before, between and after them
   arrives as today's plain paste would give it.
5. **Paste as Plain Text** (review O10): `Ctrl+Shift+V` (`Cmd+Shift+V` on macOS), also in the Edit
   menu and the editor's context menu, always pastes the plain text without conversion, as
   Obsidian does.

Example (invented): a table with three columns whose second row has two lines in its second and
third cell becomes

```markdown
| Product | Supplier | Price per kg |
| --- | --- | --- |
| Green tea | Tea House North<br>Tea House South | 12,50<br>13,20 |
| Black tea | Tea House West | 9,80 |
```

## Rules

- **When a paste converts.** Only `Ctrl+V` (`QKeySequence::Paste`, also Edit › Paste and the
  context menu) in the editor text, with the caret outside a fenced code block, front matter and a
  table's cells. In a code block or front matter, in a cell of a shown table (S018) and on the
  Markdown lines of a table (source view `Ctrl+Shift+M`, or a row being typed), the plain text is
  pasted as today (review O7). Drag and drop is not converted. `Paste as Plain Text` never
  converts (decision 5).
- **What counts as a table, HTML.** A table of the HTML flavour with at least two rows and two
  columns. A smaller table counts as text: its cells' lines, row by row, one line each. Without
  any table that counts, the whole paste is plain text as today.
- **Text around tables** (HTML). Text outside the tables (before, between, after) is kept as plain
  text: each paragraph and each line break becomes one line, without formatting and without
  escapes, as today's plain paste. Whitespace at the start and end of each line is dropped; a
  no-break space (U+00A0) counts as whitespace. Empty lines at the start and end of each text part
  are dropped; inside a text part they stay.
- **What counts as a table, plain text** (only without an HTML flavour): a field is quoted only when
  it starts with `"`, its closing `"` stands directly before a tab, a line break or the end of the
  text, and the text between them holds a line break or a tab; inside it `""` is one `"`. Every
  other `"` is text (`"Leaf" Co`, `say "hi"`, `""x`; revised after the Excel samples, Armin,
  2026-11-09). The text is split into rows at line breaks outside quoted fields (a final line break
  ends the last row) and into fields at tabs outside quoted fields; at least two rows, every row
  with the same number of fields, at least two; not every row starting with a tab (indented text).
  Otherwise plain text as today.
- **Header.** The first row is the header row, whether the source marks it or not.
- **Cell text.** Plain text without formatting: bold, italics, links, colours, images and
  footnotes of the source are dropped, their text stays (a link keeps its text, not its target). An
  image without text leaves nothing.
- **Lines in a cell.** Each line break and each paragraph boundary inside a cell becomes one `<br>`;
  empty lines at the start and end of a cell are dropped, empty lines between two lines stay as
  `<br><br>`. Whitespace at the start and end of each line is dropped; a no-break space (U+00A0)
  counts as whitespace and inside a line becomes a normal space. A cell with nothing left is empty
  (`|  |`, as S026 writes empty cells).
- **Escaping.** The cell shows exactly the copied text: `|` becomes `\|`, and `\`, `` ` ``, `*`,
  `_`, `~`, `[`, `]` and `<` are escaped with a backslash (S020); `&` when `#` follows, or letters
  and digits and then `;` (`&amp;`, `&#38;`, `&foo;`; review O9: also names that are no
  character reference, which is harmless; `R&D` and `A & B` stay). A literal `<br>` in the source
  thus stays text. Other characters stay as they are (block syntax such as `#` or `>` has no
  effect in a cell).
  The cell with the caret shows its Markdown with the backslashes, as S018's cell editor shows
  every cell's Markdown; the other cells show the text (review O2, accepted as documented).
- **Merged cells.** A cell spanning several columns or rows keeps its text in its top-left cell;
  the other cells it covers are empty.
- **Nested tables.** A paste with a table inside a cell of a table is not converted: the whole paste
  arrives as plain text, as today (Armin, 2026-11-09: Markdown has no nested tables, and they come
  mostly from layout HTML such as newsletters, where the outer table holds no data).
- **Width.** Every row gets as many cells as the widest row; missing cells are empty. Columns or
  rows that are empty throughout stay (they were copied).
- **Where it lands.** Each table is its own block: the paste replaces the selection, then, if the
  caret is inside a line, the line is split there. Between a table and text before or after it
  (text of the document or of the paste) is exactly one blank line; at the start or end of the
  document and next to an existing blank line none is added. Several tables are separated by one
  blank line. With the caret on the blank line directly below a table, this rule replaces S018's
  `TableEmbedder::beforeWritingBelow` for the paste: exactly one blank line separates the two
  tables (review O7). Pasted text after the last table continues on the line the caret was split
  at, as a plain paste would. New lines take the majority line ending (ADR 0002). The caret ends
  after the last pasted character.
- **One step.** `Ctrl+Z` undoes the whole paste in one step, redo (`QKeySequence::Redo`) brings
  it back.
- **Nothing else changes:** the pasted Markdown is the only change to the document; the file is
  saved byte for byte as always (ADR 0002). The table then behaves as any table (S018).
- **Same result at every size, time linear in the cells** (review O4 and O11, Armin, 2026-11-09).
  There is no size limit: a table is converted by the same rules whether it has four cells or
  fifty thousand. A paste of up to 500 cells takes at most 50 ms on the CI runner, like a key
  press; above that at most 100 µs per cell (5,000 cells 500 ms, 50,000 cells 5 s), measured from
  the paste until the table is shown. Cells count over all pasted tables. While a paste converts,
  hitokoto shows the system's wait cursor. Blocking the UI for a large paste is a recorded
  exception to the brief, section 4 (ADR 0011).

## Acceptance scenarios

**Scenario: a table from Outlook becomes a Markdown table**
- Given the clipboard holds the Outlook sample of the example table from the templates (HTML and
  plain text, three columns, one row with two lines in two cells)
- And the caret is on an empty line between two paragraphs
- When I press `Ctrl+V`
- Then the document holds the table of the example, with one blank line before and after it
- And it is shown as a table (S018), the two-line cells on two lines

**Scenario: text before and after a table from Outlook**
- Given the clipboard holds the Outlook sample with text from the templates: a paragraph before the
  table, the table, and a line after it (text with a line break, no paragraph of its own)
- And the caret is on an empty line between two paragraphs
- When I paste it
- Then the document holds the line before, a blank line, the table, a blank line and the line
  after, each line as plain text without escapes

**Scenario: a table from Excel, with and without HTML**
- Given the clipboard holds the Excel sample from the templates (HTML and Excel's plain text, a
  cell with a line break and a cell with `"`)
- When I paste it
- Then the table holds the cells of the sample, the line break as `<br>`
- And the same Markdown results when the clipboard holds only Excel's plain text
- Given the clipboard holds only Excel's plain text of the quotes sample (cells `"Leaf" Co`,
  `say "hi"`, `a "b"` with a line break, and `"a` and `b"` on two lines)
- When I paste it
- Then each cell holds its text as typed in Excel

**Scenario: a table from a web page and from Word**
- Given the clipboard holds the samples from the templates in Firefox (a table with `<th>`, a link
  and bold text in cells) and from Word (merged cells)
- When I paste each
- Then the link keeps its text only, bold is dropped, merged cells keep their text in the top-left
  cell and leave the others empty

**Scenario: the table lands as its own block**
- Given the caret in the middle of a paragraph line, or at its end, or at the start of the
  document, or with text selected across two lines
- When I paste a table
- Then the text before the caret stays on its line, a blank line follows, then the table, a blank
  line and the rest of the line; no blank line at the document's start or end and none doubled
  next to an existing blank line; a selection is replaced first
- And the caret stands after the last pasted character, here the table's last row

**Scenario: next to an existing table**
- Given the caret on the blank line directly below a shown table
- When I paste a table
- Then exactly one blank line separates the existing and the pasted table
- Given the caret on a table's Markdown lines (source view, or a row being typed)
- When I paste a table
- Then the plain text is pasted as today and the existing table keeps its rows

**Scenario: escaping keeps the text as copied**
- Given a copied table whose cells hold `a|b`, `*x*`, `C:\Dev`, `` `c` ``, `R&D`, `&amp;`,
  `[l](u)` and `x<br>y` as text
- When I paste it
- Then each cell without the caret shows exactly that text and the Markdown holds `a\|b`,
  `\*x\*`, `C:\\Dev`, ``\`c\` ``, `R&D`, `\&amp;`, `\[l\](u)` and `x\<br>y`
- And the cell with the caret shows its Markdown, backslashes included, as S018's cell editor
  does for every cell (review O2; like the active line in S020)
- And `x<br>y` stays text in both: no line break in the cell with the caret (`x\<br>y`), and
  `x<br>y` in the others (review O3)

**Scenario: what stays plain text**
- Given the clipboard holds HTML whose only table has one column or one row, or a table with a
  table inside a cell, or no HTML and plain text with rows of different field counts, or
  tab-indented lines only
- When I paste it
- Then the plain text is pasted as today
- Given the caret in a fenced code block, in front matter or in a cell of a shown table
- When I paste a table
- Then the plain text is pasted as today (in a cell, as S018 writes pasted text)

**Scenario: paste as plain text**
- Given the clipboard holds a table from Outlook or Excel's plain text
- When I press `Ctrl+Shift+V` (`Cmd+Shift+V` on macOS) or choose Edit › Paste as Plain Text or the
  same entry in the editor's context menu
- Then the plain text is pasted as today, without conversion
- And the entry is disabled when the clipboard holds no text

**Scenario: one undo step**
- Given a pasted table
- When I press `Ctrl+Z`
- Then the document is as before the paste, the caret at the place of the paste (as after undoing
  any paste in hitokoto; a replaced selection is not selected again, review O8)
- And redo (`QKeySequence::Redo`) brings the table back

**Scenario: paste time grows linearly with the cells**
- Given copied tables with Outlook's and with Excel's markup, of 50 × 10, 500 × 10 and 50 × 100
  cells
- When I paste each
- Then 50 × 10 is shown within 50 ms, 500 × 10 and 50 × 100 within 500 ms
- And each is converted by the same rules as the small samples (no size limit)
- And the wait cursor shows while the paste converts and is gone afterwards

## Theming

No new visible element: the pasted table is an S018 table with its tokens. No new tokens.

## Not in this story

- Converting formatting (bold, links, lists, headings) in the text around tables; it arrives as
  plain text. Goes to `unrefined.md` if wanted.
- Column alignment from the source (`text-align`), padding to column widths.
- Pasting a table into a cell of a table, drag and drop.

## Requirements for the implementation

- **Core** (standard library only): a pasted-table model (rows of cells, each a list of lines) and
  its Markdown (`pastedTablesToMarkdown`), the escaping, the plain-text parser for Excel's form and
  the placement (blank lines around the tables at a given position). Checked against cmark-gfm
  (`cmarkgfm`, GFM 0.29) and markdown-it-py with tables before the tests are written: each table
  parses as a table with the copied cell text.
- **UI:** `EditorWidget::insertFromMimeData` and `canInsertFromMimeData` decide; the HTML is read
  with Qt's HTML import (`QTextDocumentFragment::fromHtml` or `QTextDocument::setHtml`, QtGui, no
  new module) and its `QTextTable` cells walked into the core model (ADR 0011). On Windows Qt
  delivers the clipboard's "HTML Format" as `text/html` without its header.
- **Test data** (review O5, Armin, 2026-11-09; one template instead of three, Armin, 2026-11-09):
  only tables made for the tests, with invented content, from one text template in the repository,
  `tests/data/s031/templates/tables.html`. Firefox opens it directly, Word and Excel open the same
  file; Outlook classic gets the table from Word, pasted into a new mail and copied again there. The
  clipboard contents are taken on Windows. The template holds cells with `<br>` and with two `<p>`,
  `colspan` and `rowspan`, and for Excel `<br style='mso-data-placement:same-cell'>` (a line break
  Excel keeps inside the cell). The tests need only the clipboard samples; binary formats (`.docx`,
  `.xlsx`) would add nothing. Nothing from real mails or documents enters the repository, also
  nothing derived from it. Before a sample is committed, its header is reduced: no `file:` links,
  `<link>` elements, generator or author. The lint test `S031_SamplesCarryNoPersonalData` fails on
  `file:`, `C:\Users`, `/Users/`, `/home/` or an e-mail address under `tests/data/s031/`. Benchmarks
  generate their large tables from the markup pattern of these samples.
- **S018's cell helpers** (review O3): `core::tableCellToText` and `TableDelegate::prepare` treat a
  `<br>` preceded by an odd number of backslashes as text, not as a line break. Red tests first in
  `S018_Tables` and `tst_s018_tables`; this also fixes typing `\<br>` into a cell today. S028's
  search reads cells by the same rule (`find.cpp`, red test in `S028_FindInFile`; Armin,
  2026-11-09).
- **S009** is unaffected: nothing new is concealed.

## Tests

- Core: `S031_PasteTables` for the Markdown, escaping, lines in cells, merged cells, nested tables
  (plain text), width, text around tables, plain-text parsing (accepted and refused inputs) and
  placement.
- UI: `tst_s031_paste_tables` for every scenario above, pasting through `QMimeData` with the
  samples from `tests/data/s031/`.
- Fuzzing (review O6): a target `fuzz_paste` in `fuzz/` over the plain-text parser, the Markdown
  of a random cell model and the placement; a run without findings is one of the S031 gates. Qt's
  HTML import is not fuzzed (Qt's code; it would need Qt in the fuzz build).
- Benchmarks (hard budgets, Outlook and Excel markup each): `s031_paste_outlook_50x10_ms` and
  `s031_paste_excel_50x10_ms` (50 ms), `s031_paste_outlook_500x10_ms`,
  `s031_paste_excel_500x10_ms`, `s031_paste_outlook_50x100_ms` and `s031_paste_excel_50x100_ms`
  (500 ms). Spike 2026-11-09 (Qt 6.12, cloud VM): Qt's HTML import about 40 µs per cell with
  Outlook's markup and 12 µs with Excel's, the Markdown about 1 µs; embedding estimated from S018
  at about 12 µs. If the budget fails with Outlook's markup, report the numbers before changing it.
  The six keys are hard budgets of the benchmark gate (S029). Main does not measure them yet, so
  the gate treats a hard key that neither base round holds, while the pull request's rounds do,
  as new: the base rounds are complete without it, the pull request's rounds still need every hard
  key and their values are checked against the budgets (Armin, 2026-11-09; gate cases
  `new_hard_key`, `new_hard_key_missed`, `new_hard_key_one_base_round`).
