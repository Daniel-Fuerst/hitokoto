# ADR 0011: Tables from the clipboard's HTML, read with Qt's HTML import

- **Status:** Accepted (Armin, 2026-11-09, with the confirmation of S031)
- **Date:** 2026-11-09
- **Deciders:** project owner (Armin, Daniel)

## Context

S031 turns a table pasted from Outlook, Excel, Word or a web page into a GFM pipe table. These
programs put several flavours on the clipboard; the two that matter are plain text and HTML.

Plain text is not enough. Outlook writes a table as lines with cells separated by tabs, and a line
break inside a cell as a raw line break, without quotes. In Armin's sample of 2026-11-09 a row
whose second and third cells hold two lines each arrives as three lines, the second of them
holding the end of one cell and the start of the next; the rows cannot be told apart again. Excel
quotes such cells, but Outlook, Word and browsers do not. Only the HTML flavour keeps rows, cells
and lines in a cell apart.

The brief (section 2) rules out web technology: "No HTML, web-style CSS, browser engines, webviews".
Its purpose is the user interface and the stack: hitokoto renders nothing as HTML and runs no
browser engine. Reading the HTML flavour is input only, but it is HTML, so this needs a decision
(CLAUDE.md: decisions that conflict with the brief are asked for and recorded). The core uses the
C++ standard library only; the brief (section 3) asks to minimise dependencies.

## Options

1. **Plain text only.** No HTML at all. Tables from Excel work (quoted cells); tables from Outlook,
   Word and browsers with several lines in a cell break, which is the case that started S031.
2. **Qt's HTML import in the UI.** `QTextDocumentFragment::fromHtml` (or `QTextDocument::setHtml`)
   from QtGui, which hitokoto links already, parses the HTML into a document with `QTextTable`s:
   cells, row and column spans, paragraphs and line breaks (`<br>` as U+2028). The UI walks the
   tables into a small core model; the core decides whether the paste is a table and writes the
   Markdown. No new module, no new dependency. The parsing itself is Qt's and is only as good as
   Qt's HTML subset (Word's and Outlook's `mso-` styles are ignored, which is fine for text).
3. **Own HTML table reader in the core.** Testable without Qt, but HTML as Office and browsers write
   it (implied end tags, entities, conditional comments, `<o:p>`) is a parser of its own to
   maintain, for one feature.
4. **A third-party HTML parser** (lexbor, gumbo-parser). Complete HTML5 parsing, but a new
   dependency on three platforms; Google's gumbo repository is archived (forks exist). Against the
   brief, section 3, for a paste feature.

## Decision

Option 2. The HTML flavour of the clipboard is read only on paste and only to find tables; nothing
is rendered, no resource (image, stylesheet, script, link) is loaded or followed, and the Markdown
text stays the only document (brief, section 1). There is no size limit: the result is the same
for every size, and the time grows linearly (S031: 400 cells in 50 ms, then at most 125 µs per
cell; first 500 cells and 100 µs, changed by Armin on 2026-11-10 after the CI run with
sample-like cells).

- UI: `EditorWidget::insertFromMimeData` asks the core whether to convert; it builds the core model
  from the `QTextTable`s of the imported fragment and from the text outside them (kept as plain
  text, S031).
- Core: the model (rows of cells, each a list of lines, with spans), the rules of S031 (header,
  lines, escaping, width, merged and nested cells), Excel's plain text and the placement of the
  tables in the document. All of it is tested without Qt.
- The UI part is tested with real clipboard contents from Outlook classic, Excel, Word and Firefox,
  taken from test tables with invented content (`tests/data/s031/`, S031 review O5).

## Consequences

- **Exception to the brief, section 4** ("Never block the UI thread with … parsing of large
  documents"): the import runs on the UI thread, so a large paste blocks it, linearly in its cells
  (spike 2026-11-09: about 40 µs per cell with Outlook's markup on the cloud VM; 50,000 cells take
  seconds). Accepted by Armin on 2026-11-09 (review O4): the result must not depend on the size,
  and importing in a worker thread would need `QTextDocument` off the UI thread and rules for
  typing while a paste is pending. hitokoto shows the wait cursor while a paste converts.

- No new dependency and no web technology in the user interface; the brief's section 2 holds in its
  purpose. This ADR is the recorded exception for reading HTML as clipboard input.
- What Qt's HTML import does not understand is lost (for example a table built from `<div>`s with
  CSS grid instead of `<table>`); such pastes stay plain text.
- The conversion depends on Qt's HTML parser, which may change between Qt 6.8 and 6.12; the UI
  tests run with both.

## Implementation (S031, 2026-11-09)

- `ui::readPastedHtml` (`src/ui/table_paste.cpp`) imports into a `QTextDocument` whose
  `loadResource` returns nothing: a stylesheet `<link>` or an image is never read, also not from a
  local file. The document is never laid out or shown. The HTML loses its style attributes first
  (`core::htmlWithoutStyles`, 2026-11-10): Outlook's inline CSS was half of the import, and the
  tables and their text stay the same.
- Before the import, `core::scanHtmlTables` scans the HTML once for table tags (comments
  skipped): HTML without a table, or with a table inside a table, is never imported and stays plain
  text (review (code) O1: Qt's import of deep nesting is far from linear, 64 s at depth 10,000;
  O9: most copies carry HTML without a table).
- The core (`hitokoto/core/paste.hpp`) decides and writes; the UI inserts the Markdown with one
  `QTextCursor::insertText`, so one undo step, and shows the wait cursor meanwhile, until the
  table embedder has shown the pasted table (review (code) O6).
- Measured on the development VM (`docs/performance.md`): 500 cells with Outlook's markup 44 ms until
  the table shows, of which the import 20 ms. On the CI runner, with cells like the samples' and
  after the review fixes, 500 cells with Outlook's markup took 48–49 ms and 5,000 cells (50 × 100)
  492–502 ms; the benchmark tables are now 40 × 10, 400 × 10 and 40 × 100 (Armin, 2026-11-10).

## Reconsider if

- Large pastes are common enough that the blocking disturbs; then the import moves off the UI
  thread.

- The real samples show that Qt's import loses rows, cells, spans or lines in a cell.
- More HTML is to be converted than tables (bold, links, lists); then the reader's scope and this
  decision are revisited.
