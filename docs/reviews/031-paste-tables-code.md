---
story: S031
mode: code
spec: docs/stories/031-paste-tables.md
commits: 0a224dc..2178d93
date: 2026-11-09
reviewer: Advocatus Diaboli (independent subagent)
dispositioned: 2026-11-09 (Armin, one objection after the other; O5 after a look at middle-click pastes in Qt and LibreOffice, O8 after the runner/VM ratios before and after the S018 sizing fix; the mechanism of O9 and steps 2 and 3 of O10 on 2026-11-10 after measurements)
objections:
  - id: O1
    category: threat
    severity: minor
    title: "Nested-table HTML is imported in super-linear time before the core decides that it stays plain text"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Before the import the HTML is scanned once for table start and end tags; a table inside a table means plain text without importing. The same scan skips the import when the HTML holds no table at all (O9). Red test first: a table nested 3,000 deep stays plain text well within a second."
  - id: O2
    category: failure
    severity: major
    title: "Ctrl+Shift+V in a table cell or the find field pastes into the document at the editor's caret"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Ctrl+Shift+V acts where the focus is: in a cell editor the plain text goes into the cell (as Ctrl+V there, S018), in the find field into the field, in the editor as before; the document changes only as far as the cell asks. Red tests first: Ctrl+Shift+V in a cell and in the find field."
  - id: O3
    category: failure
    severity: minor
    title: "A cell starting with a block marker ('1. Jänner', '- 5 %', '# 12', '> 5 kg') is shown as a list, heading or quote"
    disposition: fixed
    disposition_rationale: "Display in S018: TableDelegate escapes a leading block marker (#, >, -, +, ordered-list start) only for rendering, so a cell shows its characters as GFM reads them; the file stays unchanged. Helps typed and pasted cells alike. Red test in tst_s018_tables, expectation checked with cmark-gfm."
  - id: O4
    category: failure
    severity: minor
    title: "Splitting a line inside a blockquote or list item moves the rest of the line out of its container and can merge the next list item into a paragraph"
    disposition: fixed
    disposition_rationale: "Inside a blockquote or list item the paste stays plain text, as in a code block, front matter or cell; one red scenario per container (blockquote, ordered and unordered list), checked with cmark-gfm. Carrying the container prefix onto the table lines goes to unrefined.md if practice needs tables in lists."
  - id: O5
    category: failure
    severity: minor
    title: "A middle-click paste (X11/Wayland primary selection) converts tables, although only Ctrl+V should"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-09. A middle-click paste of the primary selection converts like Ctrl+V; the story's rule \"When a paste converts\" says so. On Linux a middle click is a paste from the second buffer: Qt routes it through insertFromMimeData like Ctrl+V, and rich-text applications such as LibreOffice paste it formatted. Unlike a drop, the user chose to paste; Ctrl+Z undoes an unintended one. No code change and no test: the offscreen platform has no selection clipboard."
  - id: O6
    category: failure
    severity: minor
    title: "The wait cursor is gone for the last third of a large paste, while the table is built and sized"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. On a converted paste the override cursor is restored by a queued call placed after the embedder's queued refresh, so it stays until the pasted table is shown; when pasteTables returns false it is restored at once as before. Red test first in tst_s031_paste_tables: when the view of the pasted table is shown, the wait cursor is still set, and none afterwards. The story already asks for it."
  - id: O7
    category: operational
    severity: minor
    title: "The Edit menu fetches the full clipboard text on every clipboard change of any application"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. canPastePlainText() asks only hasText(), which checks the formats and transfers no data; pasteAsPlainText still ignores empty text when it runs. The update stays on dataChanged, not aboutToShow: a disabled action also blocks its shortcut, and Ctrl+Shift+V must work in a cell and the find field (O2). Red test first: on offscreen a QMimeData subclass counts retrieveData for text/plain; after setting it the action is enabled and nothing was fetched; with an image only it is disabled."
  - id: O8
    category: operational
    severity: minor
    title: "The 50 ms hard budget has less margin than review O11 assumed; the slower runner seen before would miss it"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-09. No budget change now. docs/performance.md records the margin (Outlook 50 x 10: 35 ms on the VM, factor 1.4; 32 ms on the runner for 2178d93, factor 1.55) and the runner/VM ratios before and after the S018 sizing fix (s018_table_open 1.65 to 1.0, Outlook 50 x 10 1.63 to 0.92, Excel 50 x 10 1.91 to 1.0): the slow runner was slow mainly in the setMarkdown layout that plain cells now skip, so the factor 1.5-1.9 no longer applies to them, but still to cells that take setMarkdown (O10). If an s031 hard budget trips in a pull request, first re-run the failed jobs; if it trips again, the numbers go to Armin before any budget changes. The final numbers follow the fix of O10. Armin, 2026-11-10, after the CI run on 5a1e4d0 (Outlook 50 x 100 at 502 and 492 ms, 50 x 10 at 49 and 48 ms): the times stay, the tables shrink to 40 x 10, 400 x 10 and 40 x 100 (400 cells in 50 ms, then at most 125 us per cell)."
  - id: O9
    category: cost
    severity: major
    title: "Every paste with an HTML flavour now runs Qt's HTML import on the UI thread, also without any table and for copies from hitokoto itself"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. The scan of O1 runs before the wait cursor: HTML without a table start tag is not imported, the paste is plain text at once and shows no wait cursor. Red test first: no import for HTML without a table. A benchmark key for a large paste without a table (code with a span per token) is compared with main, without a hard budget. hitokoto's own copies: the paste recognises that the data comes from hitokoto itself and pastes plain text without asking for the HTML (preferred over copies without HTML, which would change what other programs receive); the mechanism is settled after measuring the cost of html() for an own copy in the red phase. Mechanism (Armin, 2026-11-10, after the measurement): a marker alone gained little, because Qt's data for a selection makes HTML, Markdown, ODF and plain text at the first request (Ctrl+V of 50,000 copied lines 818 to 668 ms); the editor's copy (OwnCopy) offers the same formats, each made only when asked for, plus the marker application/x-hitokoto-copy, and a paste with the marker skips the HTML (255 ms, as the plain text alone)."
  - id: O10
    category: cost
    severity: major
    title: "The benchmark tables hold one-word cells that all take the new plain-text fast path; two-line cells, the case S031 was written for, miss the budgets"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Three steps. 1: the benchmark cells are generated like the samples' cells (one word, two lines via two <p> for Outlook and <br> for Excel, text with &, : or _) for all six keys; the numbers go to Armin before code or budgets change. 2: a cell whose Markdown holds nothing but <br> and backslash escapes is laid out as plain text too (tableCellToText, line break as U+2028 in one block); red test in S018_Tables first, the guard cellSizesMatchTheMarkdownRendering gains such cells, the leading-marker escapes of O3 included. 3: measure again; if a budget still trips, Armin and Claude decide it together. Narrowing the exceptions for :, & and @ is left out for now. Armin, 2026-11-10, after the numbers of step 1: the cell mix stays; step 2 as planned (core::tableCellPlainLayout); step 3: a colon not followed by // and an ampersand that starts no entity are text too (tableCellIsPlainText), links with ://, e-mail addresses and entities keep the Markdown layout, and the risk of a link form that this misses is accepted (a follow-up story if practice needs it). Numbers in docs/performance.md."
---

# S031 — Advocatus Diaboli, mode code

Reviewed: the first-parent commits b651f22 (red tests), 4fadaa0 (implementation), 7a742f6
(`unrefined.md`), 3396887 (red test for fast cell sizing) and 2178d93 (fast cell sizing) on
`agents/s031-paste-tables`, `git diff 0a224dc 2178d93`, against `docs/stories/031-paste-tables.md`,
ADR 0011 and the dispositions of `docs/reviews/031-paste-tables-spec.md`. The merge aa0377a changes
no S031 code (`git diff --stat 2178d93 aa0377a -- src bench tests/ui tests/core` touches only
`src/app`). Code read: `src/core/src/paste.cpp`, `src/core/src/elements/table.cpp`,
`src/core/src/find.cpp`, `src/ui/editor_widget.cpp`, `src/ui/table_paste.cpp`,
`src/ui/main_window.cpp`, `src/ui/table_view.cpp`, `src/ui/table_embedder.hpp`,
`bench/hitokoto_bench.cpp`, `bench/gate/`, the tests, `fuzz/fuzz_paste.cpp` and
`tests/lint/no_personal_data.cmake`.

Evidence was gathered without touching the repository: a probe program outside it
(`scratchpad/probe/probe.cpp` of this review session) linked against the existing release build
`build/ci` (`libhitokoto_ui.a`, `libhitokoto_core.a`, the S031 sample resources; Qt 6.12.0,
`QT_QPA_PLATFORM=offscreen`, the cloud VM), driving `ui::MainWindow` through the UI tests' `Fixture`
and `test::keyClick` exactly as `tst_s031_paste_tables` does. GFM expectations were checked with
cmarkgfm. Timings are medians on the VM, which measured the S031 benchmark at 32 ms for Outlook
50 × 10, the value of the CI run on 2178d93.

## O1 - threat - minor

**Nested-table HTML is imported in super-linear time before the core decides that it stays plain
text.** A paste whose table holds a table stays plain text (rule "Nested tables"), but that is
decided by the core after `readPastedHtml` has run `QTextDocument::setHtml` over the whole HTML
(`table_paste.cpp:90-92`, `editor_widget.cpp:582-583`). Qt's import of nested tables is far from
linear in the input: a table nested `depth` times (two rows, two cells each) took, through
`ui::readPastedHtml`, 241 ms at depth 1,000 (70 KB), 2.1 s at depth 3,000 (210 KB) and 64 s at
depth 10,000 (about 700 KB); `setHtml` alone takes about 90 % of it (218 ms and 1.9 s). The
clipboard is untrusted input (spec review O6): any web page can replace what the user copies with
its own HTML in a `copy` handler, and a few hundred kilobytes then freeze hitokoto for a minute on
`Ctrl+V`, with nothing converted at the end. ADR 0011's exception to the brief, section 4, covers
blocking "linearly in its cells" for a table that converts, not this. The fuzz target does not cover
it by design (Qt's import is not fuzzed).

Evidence: `src/ui/table_paste.cpp:90-92`; `src/ui/editor_widget.cpp:582-583`; probe `nested 1000`,
`3000`, `10000` and a stand-alone `setHtml` timing (outputs above); story, rules "Nested tables" and
"Same result at every size, time linear in the cells"; ADR 0011, "Consequences".

Proposal: before the import, scan the HTML once for `<table` / `</table` (case-insensitive) and
track the depth; a depth above one means plain text without importing. This keeps the rule and
makes the decision linear. A unit test with a deep nesting and a time bound, or a fuzz-like test in
`tst_s031_paste_tables` with depth 3,000 that must finish in well under a second.

## O2 - failure - major

**Ctrl+Shift+V in a table cell or in the find field pastes into the document at the editor's
caret.** "Paste as Plain Text" is a main-window menu action with the window-wide shortcut
`Ctrl+Shift+V` bound to `EditorWidget::pasteAsPlainText` (`main_window.cpp:114-119`). Neither the
S018 `CellEditor` (`QPlainTextEdit`) nor the find field (`QLineEdit`) claims that key in
`ShortcutOverride` (it is no standard key sequence), so the action fires and inserts the clipboard
text into the main document at the editor's own caret, wherever that is. Demonstrated with the
probe:

- Cell: document `| a | b |\n| --- | --- |\n| 1 | 2 |\n\nafter\n`, cell (1, 0) in edit, clipboard
  `x\ny`, `Ctrl+Shift+V` on the cell editor. The document becomes `x\ny| a | b |\n| --- | --- |\n…`:
  the header line is now `y| a | b |` with three cells against a two-column delimiter row, so the
  table is no table any more; the cell editor is destroyed (the probe's first run crashed on the
  dangling focus pointer until it held it in a `QPointer`).
- Find field: `Ctrl+F`, clipboard `needle`, `Ctrl+Shift+V` in the field. The field stays empty and
  the document becomes `needlePara one.\n…`.

Users coming from Obsidian, browsers or chat clients press `Ctrl+Shift+V` in any text field, so
both cases are ordinary. The tests press the key only on the editor itself
(`pressPastePlain(f.editor())`, `tst_s031_paste_tables.cpp:135-139`, `:593-618`); none presses it
with the focus in a cell or in the find bar. The story says "in the editor text" for conversion; for
Paste as Plain Text it names Edit menu, context menu and the key, but nowhere that the key reaches
into the document from another widget.

Evidence: `src/ui/main_window.cpp:114-119`; `src/ui/editor_widget.cpp` (`pasteAsPlainText`); probe
modes `cell` and `find` (outputs quoted above); story, decision 5 and scenario "paste as plain text".

Proposal: red tests first (`Ctrl+Shift+V` with the focus in a cell editor and in the find field:
the document stays unchanged; in the cell the plain text goes into the cell, as `Ctrl+V` does in
S018; in the find field into the field or nothing). Then either let `CellEditor` and the find field
accept `Ctrl+Shift+V` in `ShortcutOverride` and paste plain text themselves, or make
`pasteAsPlainText` act on the focus widget (paste into it if it is a text widget other than the
editor, do nothing otherwise).

## O3 - failure - minor

**A cell starting with a block marker is shown as a list, heading or quote.** The rule "Escaping"
leaves `#`, `>`, `-`, `+` and `1.` unescaped because "block syntax such as `#` or `>` has no effect
in a cell" (story, line 83), and `escapeTableCellText` follows it (`paste.cpp:210-238`). That holds
for GFM: cmark-gfm renders `| 1. Jänner 2026 | # 12 |` and `| - 5 % | > 5 kg |` as cells with
exactly that text. It does not hold for hitokoto's display: `TableDelegate::prepare` lays out every
cell that is not plain text with `QTextDocument::setMarkdown(…, MarkdownDialectGitHub)` as a
document of its own (`table_view.cpp:174`), and `tableCellIsPlainText` deliberately sends these
cells there (`table.cpp:269-277`). Qt 6.12 then gives: `1. Jänner 2026` → a list item "Jänner
2026"; `12. März` → a list item "März"; `- 5 %` → a list item "5 %"; `+ 2 Stk.` → a list item;
`# 12` → a level-1 heading "12"; `> 5 kg` → a quote "5 kg". Dates in the form `1. Jänner`, negative
amounts written `- 5 %` and quantities like `> 5 kg` are ordinary in copied business tables, and
the scenario "escaping keeps the text as copied" promises that "each cell without the caret shows
exactly that text". The file is correct; what the user sees after the paste is not the copied text.
The display behaviour is S018's, but S031 relies on it for its promise; the UI test
(`escapingKeepsTheTextAsCopied`) covers only inline syntax.

Evidence: story, rule "Escaping" (line 83) and scenario "escaping keeps the text as copied";
`src/core/src/paste.cpp:210-238`; `src/ui/table_view.cpp:150-174`;
`src/core/src/elements/table.cpp:269-277`; a stand-alone Qt 6.12 check of `setMarkdown` on the six
cells (block text, `textList()`, `headingLevel()`) and the cmark-gfm rendering of the same cells.

Proposal: escape a block marker at the start of each cell line: `\#`, `\>`, `\-` and `\+` before a
space or tab, and `1\.` / `1\)` after the digits of an ordered-list start. All are valid backslash
escapes in GFM and show the same characters; check with cmark-gfm and markdown-it-py, then a red
test in `S031_PasteTables` and a UI test that the shown cell's text equals the copied text. The
alternative, making the delegate not interpret block syntax, is an S018 change.

## O4 - failure - minor

**Splitting a line inside a blockquote or list item moves the rest of the line out of its
container.** `placePastedBlocks` splits the caret's line and surrounds the table with blank lines
(`paste.cpp:413-440`); it knows nothing of container blocks. With the probe and the
`excel-t7-empty` sample:

- `> quoted text here`, caret after `quoted `: becomes `> quoted \n\n<table>\n\ntext here`.
  cmark-gfm: a blockquote "quoted", the table, and a paragraph "text here" outside the quote.
- `1. step one\n   more of step one\n2. step two`, caret at the end of the first line: becomes
  `1. step one\n\n<table>\n\n   more of step one\n2. step two`. cmark-gfm: the list ends with
  "step one", the table follows, and `more of step one 2. step two` is one paragraph: the second
  list item is gone, because an ordered list not starting at 1 cannot interrupt a paragraph.
- `- item one\n- item two`, caret at the end of the first item: the list is split in two around
  the table.

The rule "Nothing else changes: the pasted Markdown is the only change to the document" holds for
the bytes, but the text the user did not touch changes its structure. Pasting a table into meeting
notes kept as bullet lists is a common case; the scenarios cover paragraphs, blank lines, the
document's edges and tables only.

Evidence: `src/core/src/paste.cpp:413-440`; probe mode `place` (outputs above); cmark-gfm rendering
of the results; story, rules "Where it lands" and "Nothing else changes".

Proposal: decide one of: inside a list item or blockquote, paste plain text as in a code block (the
simplest, and `Ctrl+Z` plus moving the caret gives the table); or place the table after the
container block; or carry the container's prefix (`> `, the list item's indentation) onto the
table's lines, which GFM supports. A scenario for each container either way.

## O5 - failure - minor

**A middle-click paste converts tables.** The story converts "only `Ctrl+V`
(`QKeySequence::Paste`, also Edit › Paste and the context menu)"; drag and drop is excluded through
`dropEvent`, Paste as Plain Text through the same flag (`editor_widget.cpp:511-524`). On Linux,
Qt's middle-click paste of the primary selection also goes through the virtual
`insertFromMimeData` (`QWidgetTextControlPrivate::mouseReleaseEvent`, `qwidgettextcontrol.cpp`
1796-1802 in Qt 6.8: `MiddleButton` and `supportsSelection()` → `q->insertFromMimeData(md)`, which
`QTextEdit` forwards to the widget's override). A table selected in Firefox or LibreOffice and
middle-clicked into hitokoto therefore converts, with the wait cursor, although the user did not
use a paste command. No test covers it (offscreen has no selection clipboard).

Evidence: `src/ui/editor_widget.cpp:511-524`; Qt source `qtbase/src/widgets/widgets/
qwidgettextcontrol.cpp` (6.8 branch) lines 1796-1802; story, rule "When a paste converts".

Proposal: set `plainPaste_` for the middle button as `dropEvent` does (override
`mouseReleaseEvent` on the viewport event path, or the `mouseReleaseEvent` of the editor), or state
in the story that a primary-selection paste converts like `Ctrl+V`. Either way say it in the story.

## O6 - failure - minor

**The wait cursor is gone for the last third of a large paste.** `pasteTables` sets the override
cursor and restores it when it returns (`editor_widget.cpp:579-580`); the table view is built and
sized afterwards. With 500 × 10 cells in Outlook's markup, the probe measured: the `Ctrl+V` handler
returns after 195–200 ms with no table view yet and no override cursor; the next single event round
then blocks for 108–145 ms until the table is shown (308–340 ms in all). The story asks for the wait
cursor "while a paste converts" and measures the paste "until the table is shown"; the scenario says
"the wait cursor shows while the paste converts and is gone afterwards". The UI tests
(`largeTablesAreConvertedByTheSameRules`, `theWaitCursorShowsWhileASmallPasteConverts`) record the
cursor only in `QTextDocument::contentsChange`, i.e. during the insertion, so they pass although a
third of the blocking runs with the normal cursor.

Evidence: `src/ui/editor_widget.cpp:579-580`; `tests/ui/tst_s031_paste_tables.cpp`
(`CursorDuringChanges`); probe mode `cursor`: "Ctrl+V handler returned after 194.8 ms (view shown
then: 0, override cursor then: 0) … after the handler: 1 event rounds, longest 145.4 ms".

Proposal: keep the override cursor until the pasted table is embedded (restore it from the
embedder once the view of the pasted range exists, or embed synchronously under `NotTyping` before
returning); extend the test to check the cursor until `tableViewAt(…)` is visible.

## O7 - operational - minor

**The Edit menu fetches the full clipboard text on every clipboard change.** The enabled state of
"Paste as Plain Text" is updated on every `QClipboard::dataChanged` with `canPastePlainText()`,
which calls `data->text()` (`main_window.cpp:117-119`, `editor_widget.cpp:526-528`). `dataChanged`
fires for copies in any application while hitokoto runs, also in the background, and `text()`
transfers the whole text: on X11 a synchronous selection request to the owner (Qt waits up to its
clipboard timeout), on Windows a `GetData` that makes an owner with delayed rendering (Excel for
large ranges) produce the text at once. Copying a whole column in Excel then costs hitokoto, and
Excel, the rendering of a million lines that nobody pastes. Qt's own `canPaste` checks formats only.
The spec asks only that the entry is "disabled when the clipboard holds no text".

Evidence: `src/ui/main_window.cpp:117-119`; `src/ui/editor_widget.cpp:526-528`; story, scenario
"paste as plain text" ("the entry is disabled when the clipboard holds no text").

Proposal: decide the enabled state from `hasText()` alone, or update it in the Edit menu's
`aboutToShow` (and in `contextMenuEvent`, which already does it on demand); `pasteAsPlainText`
already checks the text when it runs.

## O8 - operational - minor

**The 50 ms hard budget has less margin than review O11 assumed.** Review O11 set "500 cells in
50 ms" from a spike that put about 900 Outlook cells into 50 ms on the VM, "about twice that margin
for slow CI runners". The implementation needs 35 ms for 500 Outlook cells on the VM
(`docs/performance.md`, after the cell sizing change), so about 700 cells fit: a margin of 1.4, not
2. The table embedding cost about 45 µs per cell instead of the 12 µs estimated (44 ms − 20 ms
import − 1.5 ms core, `performance.md` line 47). The CI run on 2178d93 measured 32 ms; the earlier
CI run on 7a742f6 was 1.5–1.9 times slower than the VM per key (72 vs 44 ms for Outlook 50 × 10,
ratio 1.64). If a runner of that speed comes again, Outlook 50 × 10 lands at about 57 ms and Outlook
50 × 100 at about 500 ms: hard budgets that fail a pull request "on any machine" (S029), including
pull requests that do not touch pasting. The gate compares with main on the same runner for
regressions, but hard budgets are absolute.

Evidence: `docs/performance.md` (S031 sections, lines 34-66); spec review O11 disposition; story,
rule "Same result at every size"; known CI numbers of 2178d93 (32 / 300 / 321 ms Outlook) and of
7a742f6 (72 / 515 / 539 ms).

Proposal: record now what happens when the budget trips on a slow runner (the story's "report the
numbers before changing it" covers the story, not later pull requests), and either take the slow
runner's ratio into the budget, or reduce the Outlook cost first (O10's measurements show where the
time goes). Note the margin in `docs/performance.md`.

## O9 - cost - major

**Every paste with an HTML flavour now runs Qt's HTML import, also without any table.**
`pasteTables` imports `source->html()` whenever `hasHtml()` is true (`editor_widget.cpp:582-583`)
and only then learns that there is no table; the plain paste follows. Most copies carry HTML: web
pages, mail, office programs, code editors, and hitokoto itself (`QTextEdit` puts `text/html`,
`text/markdown`, ODF and `text/plain` on the clipboard; probe output "clipboard formats: text/html,
text/markdown, application/vnd.oasis.opendocument.text, text/plain"). Measured with the probe
(handler time of `Ctrl+V`, text only versus the same text with its HTML):

| Paste | Text only | With HTML | HTML |
| --- | --- | --- | --- |
| 5,000 lines copied in hitokoto | 22 ms | 88 ms | 961 KB |
| 50,000 lines copied in hitokoto | 258 ms | 964 ms | 9.7 MB |
| 5,000 lines of code, a span per token | 22 ms | 149 ms | 857 KB |
| 20,000 lines of code, a span per token | 84 ms | 666 ms | 3.4 MB |

(For hitokoto's own copies the real cost is higher still: the probe passed a ready string, while
`QTextEditMimeData::html()` first generates the HTML from the fragment.) So moving a section within
a document with cut and paste, or pasting code into a paragraph, became three to eight times slower
and shows the wait cursor, for no change in the result. ADR 0011 records blocking the UI as an
exception for converting tables; for pastes that stay plain text the story says "as today". No
benchmark key covers a paste without a table.

Evidence: `src/ui/editor_widget.cpp:574-587`; `src/ui/table_paste.cpp:90-92`; probe modes `internal
5000`, `internal 50000`, `notable 5000`, `notable 20000`; story, rule "What counts as a table, HTML"
("the whole paste is plain text as today"); ADR 0011, "Consequences".

Proposal: before importing, look for `<table` in the HTML (case-insensitive; Qt's importer creates
a `QTextTable` only from a table element), and skip the import when there is none. Together with
O1 this is one linear scan. Add a benchmark key for a large paste without a table (for example the
10 MB document copied and pasted within hitokoto) with the key-press budget, or at least a regression
key against main.

## O10 - cost - major

**The benchmark tables hold one-word cells, which all take the new fast path.** The generated
tables fill every cell with `s031CellText(row, column)` = `r<row>c<col>` (`paste_samples.hpp:35-36`),
in Outlook's and Excel's markup of the first sample cell. 2178d93 made the budgets by laying out a
cell as plain text when `tableCellIsPlainText` holds (`table_view.cpp:150`), which needs letters,
digits and a few punctuation marks only; a `<br>` (every multi-line cell), `:`, `&`, `@`, `#`, `*`,
`_` or a leading `-` sends the cell back to `setMarkdown`. So the gate measures exactly the content
the optimisation was made for. With the probe (same harness as `measurePaste`, median of 7) and the
same tables with slightly richer cells:

| Table | `r1c2` | `r1c2 09:30` | `r1c2 R&D` | `r1c2<br>second line` | Budget |
| --- | --- | --- | --- | --- | --- |
| Outlook 50 × 10 | 32 ms | 40 ms | 41 ms | **56 ms** | 50 ms |
| Outlook 500 × 10 | 312 ms | 379 ms | 367 ms | **522 ms** | 500 ms |
| Excel 50 × 10 | 21 ms | 30 ms | 28 ms | 43 ms | 50 ms |
| Excel 500 × 10 | 169 ms | 219 ms | 215 ms | 361 ms | 500 ms |

The richer cells hold more characters too, but the story's budget is per cell and its rule is "the
same result at every size", not per content. Cells with two lines are the case S031 exists for
(story, "Basis": "cells with several lines fall apart"); on the VM that measures 32 ms for the
benchmark's Outlook 50 × 10, the same paste with two-line cells misses both Outlook budgets, and the
gate stays green. The spec required benchmarks "from the markup pattern of these samples"; the
markup is the sample's, the content is not.

Evidence: `tests/ui/paste_samples.hpp:35-73`; `bench/hitokoto_bench.cpp:754-786`;
`src/ui/table_view.cpp:150-174`; `src/core/src/elements/table.cpp:252-289`; probe mode `timing`
(outputs above); story, rule "Same result at every size, time linear in the cells" and "Basis".

Proposal: generate benchmark cells like the samples' cells (some with two lines via the sample's
`<br>` or two `<p>`, some with `&`, `:` or `_`), at least for the Outlook keys, and report the
numbers before changing code or budgets, as the story asks for Outlook. Then speed up the
`setMarkdown` path (for example a plain-text layout for cells whose only syntax is `<br>` and
backslash escapes, which `tableCellToText` already resolves) or reconsider the budget with Armin.

## Capped concerns

None; 10 objections, below the cap. Considered and left out for lack of evidence or weight:

- Excel's plain text with a cell `"a` next to a cell `b"` in the same row is read as one quoted
  field `a<tab>b`; the row then has fewer fields and the paste stays plain text unless every row
  has the same shape. The rule defines it so, and Excel always puts HTML on the clipboard as well.
- A pasted table with more than 255 columns is shown as text (S018's `kMaxColumns`); recorded in
  `unrefined.md`, not in the story's scenario "shown as a table", but no realistic paste was shown
  to hit it.
- Text around tables comes from Qt's block text, not from the clipboard's plain text, so list
  bullets that Outlook's plain text carries are dropped; no sample with a list around a table.
- `S031_SamplesCarryNoPersonalData` does not check for `<meta name=Generator>`, `<link>` or
  authors, which the README asks to remove by hand; the committed samples hold none (checked with
  grep: only `table of authorities` in Office's style list).
- `pasteTables` decides "code block or front matter" from `BlockStyler::renderFor`, which may be
  stale while a large document is still being styled; not demonstrated.

## Explicitly not objecting to

- **`escapedAt` and the order in `TableDelegate::prepare`.** `\|` is replaced before the `<br>`
  check, but a replaced pipe always stands between any backslash and the `<`, so the parity of
  backslashes before `<br>` is unaffected; `tableCellToText` and `find.cpp` use the same rule.
- **`tableCellIsPlainText` as such.** It is an allow-list, conservative by construction (leading
  `-`/`+`, ordered-list starts, four spaces and `www.` refused; `:`, `@`, `&`, `<`, `\` refused), and
  the content of the model is trimmed, so it never lays out as plain text what Qt would render
  differently. O3 and O10 are about the other path and about the benchmark, not about this check.
- **`NoResourceDocument`.** Refusing every resource in `loadResource` is the right guard for ADR
  0011's "nothing is loaded or followed", and the document is never laid out.
- **The gate's `newHardKeys`.** Exempting hard keys that neither base round holds while the pull
  request's rounds do is narrow (the pull request's rounds still need every hard key) and is tested
  by the three new gate cases.
- **One `QTextCursor::insertText` for the whole paste.** It gives the single undo step and the
  caret position the story asks for, and `undoAfterReplacingASelection` compares it with a plain
  paste (review O8).
- **`fuzz_paste` and its addition to `ci.yml`.** The change to `ci.yml` is the one the story's
  gate requires (review O6), and the oracle (escaping round trip, equal row widths, blocks in order,
  caret after the last block) checks the core's promises.
