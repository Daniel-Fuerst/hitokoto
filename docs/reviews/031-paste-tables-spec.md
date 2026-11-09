---
story: S031
mode: spec
spec: docs/stories/031-paste-tables.md
date: 2026-11-09
reviewer: Advocatus Diaboli (same session as the story, not an independent subagent)
dispositioned: 2026-11-09 (Armin, one objection after the other; O1 after three Outlook samples, O4 and O11 after a timing spike)
objections:
  - id: O1
    category: premise
    severity: minor
    title: "The rules are fixed before a real clipboard sample was seen; 'text outside the tables' may reject the Outlook case itself"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09, after three Outlook classic samples: the table alone (cells or whole rows selected, byte-identical HTML) holds only the table; with a line of text before and after, the fragment holds a paragraph, the table and text with a line break, which the old rule would have pasted as broken plain text. Story: decision 4 and rule 'Text around tables' keep that text as plain text (one line per paragraph or line break, no formatting, no escapes) and convert the tables; U+00A0 counts as whitespace outside tables too; a smaller table counts as text; scenario 'text before and after a table from Outlook'; samples listed under 'Test data'."
  - id: O2
    category: design
    severity: major
    title: "Backslash escapes show in the cell editor, which S018 keeps open on the current cell"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-09: accepted as the same principle as S020 (the backslash shows on the active line; the cell with the caret is the active line of a table). Escaping stays as specified, so copied text never turns into formatting. Story: rule 'Escaping' says the cell with the caret shows its Markdown with backslashes; scenario 'escaping keeps the text as copied' limited to cells without the caret. Not chosen: escaping only where emphasis could open (re-implements inline rules), escaping only pipes (Obsidian; copied *x* may turn italic), hiding escapes in the cell editor (breaks typing Markdown in cells, S018). Revisit as a story of its own if the backslashes disturb in practice; it changes only newly pasted tables."
  - id: O3
    category: failure
    severity: major
    title: "An escaped `<br>` and a backslash before a pipe do not survive S018's cell helpers"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Half of the objection was wrong: a backslash before a pipe survives; cmark-gfm reads x\\\\\\|y as x\\|y, tableCellToText gives x\\\\|y (the Markdown with the pipe escape resolved, as decided in O2) and tableCellFromText writes x\\\\\\|y back. The `<br>` half holds and is also a defect of S018 today. Story, requirement 'S018's cell helpers': tableCellToText and TableDelegate::prepare treat a `<br>` after an odd number of backslashes as text; red tests in S018_Tables and tst_s018_tables first; scenario 'escaping keeps the text as copied' checks x`<br>`y in the cell with and without the caret. Not chosen: writing a literal `<br>` as &lt;br> (leaves the S018 defect, shows the entity in the editor)."
  - id: O4
    category: failure
    severity: major
    title: "An 8 MB HTML limit contradicts 'never blocking'; no rule says where the parsing runs"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09, after a spike outside the repository (Qt 6.12, cloud VM, setHtml median of 5: Outlook markup 200 x 10 = 643 KB 80 ms, 1000 x 10 = 3.1 MB 379 ms; Excel-like markup 1000 x 10 = 1.1 MB 146 ms; linear, about 0.13 ms per KB). No size limit, because the result must not depend on the size; time linear in the cells: 500 cells in 50 ms, then at most 100 us per cell, measured until the table is shown; wait cursor while converting. ADR 0011 records blocking the UI for large pastes as an exception to the brief, section 4, with the reason and a 'Reconsider if'. Not chosen: a size limit with plain text above it (different result by size), importing in a worker thread (QTextDocument off the UI thread, typing while pending)."
  - id: O5
    category: threat
    severity: major
    title: "Anonymising the samples covers the visible text only; Office HTML carries user paths and metadata into a public repository"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09: the real data is confidential (a partner's business secrets); nothing of it, also nothing derived, enters the repository. The Outlook samples taken earlier confirmed the objection (file: links into the user profile, generator) and serve only as evidence for the findings; they are deleted afterwards. Story, requirement 'Test data': only test tables with invented content from templates in tests/data/s031/templates/ (.docx, .xlsx, .html), copied in Word, Outlook classic (via a new mail), Excel and Firefox; headers reduced before commit; lint test S031_SamplesCarryNoPersonalData; benchmarks generate large tables from the markup pattern of these samples. The story's example, which still carried the real table shortened, was replaced by an invented one before anything was pushed."
  - id: O6
    category: threat
    severity: minor
    title: "New parsers of untrusted clipboard input without a fuzz target"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Story, section 'Tests': fuzz target fuzz_paste over the plain-text parser, the Markdown of a random cell model and the placement; a run without findings is an S031 gate. Qt's HTML import is not fuzzed (Qt's code, would need Qt in the fuzz build)."
  - id: O7
    category: failure
    severity: minor
    title: "Placement is undefined on a table's Markdown lines and on the blank line below a table"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Story: on a table's Markdown lines (source view, a row being typed) the plain text is pasted as today, as in a cell; on the blank line directly below a table the placement rule replaces beforeWritingBelow, one blank line between the tables; scenario 'next to an existing table'. Armin: pasting a table into a table is rather an error case; preventing it with an error message is conceivable, not chosen now (cells already paste plain text, S018)."
  - id: O8
    category: failure
    severity: minor
    title: "Undo restoring 'the caret and selection as before' is claimed, not what QTextEdit does by itself"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Scenario 'one undo step' now expects the document as before and the caret at the place of the paste, as QTextEdit does for any paste; a replaced selection is not selected again. Not chosen: restoring the selection with own code around the undo stack (differs from every other paste)."
  - id: O9
    category: design
    severity: minor
    title: "The '&' rule needs the HTML5 entity list to decide what 'starts a character reference'"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09, to be refined with practice. Story, rule 'Escaping': & is escaped when # follows, or letters and digits and then ;. Over-escapes names that are no character reference (&foo; becomes \\&foo;), which cmark-gfm shows unchanged; R&D and A & B stay. Not chosen: the HTML5 entity list in the core."
  - id: O10
    category: design
    severity: minor
    title: "No way to paste a table as plain text; the plain-text fallback converts without HTML as evidence"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09. Story, decision 5 and scenario 'paste as plain text': Ctrl+Shift+V (Cmd+Shift+V on macOS), also in the Edit menu and the editor's context menu, always pastes the plain text without conversion, as Obsidian does; it is the way out for HTML tables and for tab-separated text that the fallback would convert unwanted. The shortcut was unused in hitokoto (checked). Not chosen: requiring a quoted field for the fallback (would exclude ordinary Excel ranges), dropping the fallback (against decision 2)."
  - id: O11
    category: cost
    severity: minor
    title: "The 50 ms budget for 200 x 10 is set without a measurement of Qt's HTML import and the table embedding"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-09, with O4: the spike puts Qt's import at about 40 us per cell with Outlook's markup and 12 us with Excel's, the Markdown at about 1 us, embedding estimated from S018 at about 12 us; about 900 Outlook cells would fit 50 ms on the VM. The budget is set with about twice that margin for slow CI runners: 500 cells in 50 ms, 100 us per cell above. Benchmarks s031_paste_{outlook,excel}_{50x10,500x10,50x100}_ms with hard budgets; if Outlook fails, the numbers are reported before the budget is changed."
---

# S031 — Advocatus Diaboli, mode spec

Reviewed: `docs/stories/031-paste-tables.md` and `docs/adr/0011-tables-from-the-clipboard-html.md`
at 7fcd4c6 (tree 6ab0bc2e), with S018, S020, S026, ADR 0002, ADR 0005 and the code they name
(`src/core/src/elements/table.cpp`, `src/ui/table_model.cpp`, `src/ui/table_view.cpp`,
`src/ui/editor_widget.cpp`, `src/ui/table_embedder.hpp`). Written in the same session as the story;
the project's earlier spec reviews used an independent subagent.

## O1 - premise - minor

**The rules are fixed before a real clipboard sample was seen.** The story's rules on what counts
as a table ("text outside the tables (also in elements around them) makes the whole paste plain
text") and on cell lines are derived from what Outlook, Excel, Word and browsers are expected to
put on the clipboard; the samples come later (requirement "Test data"). If Outlook's fragment for a
selected table includes the paragraph after it, or a heading, the all-or-nothing rule turns the
very case of the feature request into plain text. The rule on whitespace outside tables also names
"empty paragraphs, line breaks and whitespace", but not U+00A0, which Office writes as `&nbsp;` into
otherwise empty paragraphs; only the cell rule counts it as whitespace.

Evidence: story, rules "What counts as a table, HTML" and "Lines in a cell"; requirement "Test
data". Proposal: take the Outlook sample before the story is confirmed, and let U+00A0 count as
whitespace outside tables too.

## O2 - design - major

**Escapes show in the cell editor.** S031 escapes `\`, `` ` ``, `*`, `_`, `~`, `[`, `]` and `<` so
that "each cell shows exactly that text". S018 shows a cell through `TableDelegate::prepare`
(`setMarkdown`, which resolves the escapes), but edits it through `Qt::EditRole`, and both roles
return `core::tableCellToText`, which only turns `\|` into `|` and `<br>` into a line break
(`table_model.cpp:155-157`, `table.cpp:240-268`). S018 keeps the current cell's editor open
("The current cell's editor stays open and writes every keystroke at once"). So the cell with the
caret shows `C:\\Dev`, `\*x\*` and `snake\_case`, and every cell does once it is entered. The
scenario "escaping keeps the text as copied" holds only for cells without the caret, and pasted
text from Outlook (paths, `_`, `*` in product names) gets noisy where Obsidian writes it bare.

Evidence: `src/ui/table_model.cpp:155-157`; `src/core/src/elements/table.cpp:240-268`; S018,
"Implementation". Proposal: decide between escaping only what changes the meaning (S031 then needs
the inline rules to tell, e.g. `*` only where it would open emphasis) or making the cell editor
show escaped characters without their backslash (a change to S018's helpers, then part of S031's
scope).

## O3 - failure - major

**An escaped `<br>` and `\` before `|` do not survive S018's helpers.** `tableCellToText` turns
every `<br>` into a line break without looking at a preceding backslash, so S031's `x\<br>y`
arrives in the editor as `x\`, line break, `y`; `TableDelegate::prepare` rewrites every `<br>` to
`<br/>` before `setMarkdown`, also after a backslash (`table_view.cpp:148-152`). A copied `x\|y`
escapes to `x\\\|y`; `tableCellToText` reads the last `\|` as a pipe and keeps the other two
backslashes, giving `x\\|y` in the editor. The scenario "escaping keeps the text as copied" names
`x<br>y`; it fails with the helpers as they are.

Evidence: `src/core/src/elements/table.cpp:240-268`; `src/ui/table_view.cpp:148-152`; story,
scenario "escaping keeps the text as copied". Proposal: make the cell helpers respect backslash
escapes (red tests in `S018_Tables`), or choose another spelling for a literal `<br>` in a cell
(`&lt;br>`), and list the helper change in the story.

## O4 - failure - major

**8 MB versus "never blocking".** The story allows HTML up to 8 MB and asks that larger pastes
"must not stall the UI for longer than the budgets in `docs/performance.md` allow", but it does not
say how: `insertFromMimeData` runs on the UI thread, and the ADR builds a `QTextDocumentFragment`
there. Parsing several megabytes of Office HTML (styles on every cell) into a `QTextDocument` and
then embedding the table (S018: opening a 1000-row table 110–130 ms on the VM) takes far longer
than the 50 ms per key press the benchmark gate holds (`docs/performance.md`; brief, section 4: the
UI never blocks). Either the limit is much lower, or the parsing moves off the UI thread (is
`QTextDocument` usable there for HTML import? it touches fonts), with a defined state while it
runs (what if the user types before the table arrives?).

Evidence: story, rule "Never blocking"; ADR 0011, "Decision"; S018, "Implementation" (budgets on
the development VM). Proposal: measure Qt's import on samples of 200, 1,000 and 5,000 rows first,
then fix either a lower limit with plain text above it, or a background path with its rules.

## O5 - threat - major

**Anonymising more than the visible text.** The requirement says the samples are "anonymised (no
names, rates or customers)". Office clipboard HTML carries more than the cells: `<meta
name=Generator>`, `<link rel=File-List href="file:///C:/Users/<user>/AppData/Local/Temp/msohtmlclip…">`
links into the user profile, style names, sometimes the author, and the CF_HTML header can hold a
`SourceURL`. The repository is public; a sample committed once stays in its history. The story does
not say who checks the markup, nor that raw samples never enter the repository or the cloud copy's
git history.

Evidence: story, requirement "Test data"; hitokoto-Wissen: the repository is public (Armin,
2026-10-31); Microsoft's description of the HTML clipboard format (header fields incl.
`SourceURL`). Proposal: a rule that raw samples stay outside the repository (spike folder), that
the committed samples are reduced to the structure the tests need (tables, cell markup, spans) and
checked by a test for paths (`file:`, `C:\Users`, `/Users/`, `/home/`) and e-mail addresses.

## O6 - threat - minor

**No fuzz target.** The story adds a parser of Excel's plain text and a model built from arbitrary
HTML, both from untrusted input (any program can put anything on the clipboard). The project fuzzes
its other parsers (`fuzz/fuzz_codec.cpp`, `fuzz_render.cpp`, `fuzz_theme.cpp`), and the story's
tests list none for S031.

Evidence: `fuzz/`; story, "Tests". Proposal: a `fuzz_paste` target over the plain-text parser and
the Markdown writer (random cell model), part of the S031 gates.

## O7 - failure - minor

**Placement next to tables.** The rule "the line is split there" and "exactly one blank line" do
not cover two cases from S018: the caret on a table's Markdown lines (source view `Ctrl+Shift+M`,
or a table shown as its lines while a new row is typed), where splitting a row line breaks the
table; and the caret on the hidden blank line directly below a table, where
`EditorWidget::insertFromMimeData` already calls `TableEmbedder::beforeWritingBelow`, which adds a
blank line before the paste, so S031's own blank line would double it.

Evidence: `src/ui/editor_widget.cpp:502-506`; `src/ui/table_embedder.hpp:56-58`; S018, scenarios
on the source view and on writing below a table. Proposal: on a table's Markdown lines, paste plain
text (as in a cell) or insert after the table; below a table, the placement rule replaces
`beforeWritingBelow` for this paste; a scenario for each.

## O8 - failure - minor

**Undo and the earlier selection.** The scenario "one undo step" expects "the caret and selection
as before". `QTextEdit` undo restores the text and puts the caret at the undone change; it does not
by itself bring back a selection that the paste replaced. Nothing in the story says the
implementation restores it, and S026 asks only for one undo step.

Evidence: story, scenario "one undo step"; S026, scenario "creating a new table from the toolbar"
(`Ctrl+Z` undoes the insertion in one step). Proposal: keep "the document as before, the caret at
the paste position", or state that the selection is restored and how.

## O9 - design - minor

**`&` "only where it starts a character reference".** CommonMark recognises an entity reference
only for names in the HTML5 entity list (`&copy;` is one, `&foo;` is not) and numeric references.
To escape exactly those, the core needs the list (2,000+ names) or accepts a broader rule. The story
does not say which; the scenario only covers `&amp;`.

Evidence: story, rule "Escaping"; CommonMark 0.31.2, 2.5 "Entity and numeric character
references". Proposal: escape `&` before `#` or before a run of letters and digits followed by
`;` (over-escapes `&foo;` harmlessly), and say so.

## O10 - design - minor

**No plain paste, and conversion without HTML.** The story leaves `Ctrl+Shift+V` out. Without it,
the only way to get a copied table as text is to paste it into a code block and move it out, or to
undo and retype. The plain-text fallback converts tab-separated text that no HTML confirms as a
table; on Linux and macOS, terminals and simple editors put only plain text on the clipboard, so
two columns of tab-separated log output become a table. The rule "not every row starting with a
tab" catches indented code, not data.

Evidence: story, rules "What counts as a table, plain text" and "Not in this story". Proposal:
include `Ctrl+Shift+V` (`Paste as Plain Text`, Obsidian's binding), or limit the fallback to text
that Excel's quoting makes unambiguous (at least one quoted field) — Armin to decide.

## O11 - cost - minor

**A budget without a measurement.** "200 rows and 10 columns in at most 50 ms" covers Qt's HTML
import, the walk over `QTextTable`, the core's Markdown, the insert and S018's embedding of a
200-row table. Excel's HTML for that table is several hundred kilobytes (a style per cell). None of
the parts is measured; S018's table opening alone is 110–130 ms for 1,000 rows on the VM, so the
budget may be met or may fail from the start, and the story is sized M on that assumption.

Evidence: story, rule "Never blocking" and scenario "large tables do not block"; S018,
"Implementation". Proposal: a spike outside the repository with Qt 6.12 and 6.8.3 on a generated
Excel-like HTML table, before the story is confirmed; set the budget from it.

## Capped concerns

None; 11 objections, below the cap. Considered and left out for lack of evidence:

- A merged title row from Excel becomes the header row with empty headers; acceptable by decision
  "Header", Obsidian does the same (not checked).
- Bullet lists inside cells lose their markers (Qt's import makes list items blocks); no sample
  shows lists in cells yet.

## Explicitly not objecting to

- Reading the clipboard's HTML through Qt's import (ADR 0011, option 2): no new dependency, no
  rendering, and plain text cannot carry Outlook's multi-line cells.
- Minimal Markdown without padding and alignment: it keeps the paste small and the file readable in
  any GFM renderer; S018 aligns columns on screen anyway.
- The first row as header: GFM requires one, and the copied table's first row is the natural one.
- No new theme tokens and no S009 change: the pasted table is an S018 table.
- Excluding code blocks and front matter from conversion: there the text must arrive as copied.
