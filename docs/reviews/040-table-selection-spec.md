---
mode: spec
spec: docs/stories/040-table-selection.md
reviewer: Advocatus Diaboli (Claude, in the session that drafted the story; not an independent
  subagent)
date: 2026-11-21
objections:
  - id: O1
    category: failure
    severity: major
    title: Shift+Space selects a row while the caret types in a cell, and the next letter replaces a cell
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): typed text, a paste of text and committed input method text replace no
      selected cell; the selection ends and the text goes where the caret was before it began
      (decision 11). A Shift held too long costs a space, not a cell.
  - id: O2
    category: design
    severity: major
    title: Preferring text/markdown on paste also takes other programs' Markdown, which can differ from their text
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): text/markdown is preferred only for hitokoto's own copies, which carry the
      marker application/x-hitokoto-copy; other programs paste as before (S031). Moved to S041,
      decision 5. Armin adds that Obsidian puts only HTML and text/plain on the clipboard, its
      text/plain being the Markdown without blank lines (S041, basis).
  - id: O3
    category: premise
    severity: minor
    title: The end cell is marked and typed into, where Excel keeps the start cell active
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): Word, not Excel, is the model. No cell is marked as the active one; a cell
      selected as a cell shows the selection colours over its whole area, text shows as text
      (decisions 1 and 2). Escape ends the selection; an arrow key without Shift moves the caret as
      if there had been no selection, from where it was before; Tab, Shift+Tab and Ctrl+Shift+M
      count from there (decision 9). The end cell's frame and its two tokens are gone.
  - id: O4
    category: failure
    severity: minor
    title: The plain text on the Windows clipboard has CRLF, not LF
    disposition: fixed
    disposition_rationale: >-
      text/markdown holds LF, checked by a test on all three platforms; should a platform convert
      it, its convention applies; text/plain follows the platform, as Qt writes it (S041, decision
      3; Armin, 2026-11-21).
  - id: O5
    category: failure
    severity: minor
    title: The find bar edits nothing; what ends a selection of cells is not listed correctly
    disposition: fixed
    disposition_rationale: >-
      Decision 9 lists what ends a selection: undo, redo, the add and delete controls of S018,
      Ctrl+Shift+M and a table rebuilt after a change of its lines; the find bar is no longer named
      (Armin, 2026-11-21).
  - id: O6
    category: failure
    severity: minor
    title: Pasting cells can need more than the 255 columns a table may have
    disposition: fixed
    disposition_rationale: >-
      A paste that would need more than 255 columns changes nothing (S041, decision 7; Armin,
      2026-11-21).
  - id: O7
    category: failure
    severity: minor
    title: Delete Row and Delete Column with every row or column selected are not defined
    disposition: fixed
    disposition_rationale: >-
      With every column selected, Delete Column keeps the first; with only the header row selected,
      Delete Row is disabled and its keys do nothing; with the whole table selected, Delete Row
      removes every body row (decision 10). Armin (2026-11-21): to be tried in practice; in addition
      Delete and Backspace with the whole table selected remove the table with its header and the
      blank line below it.
  - id: O8
    category: design
    severity: minor
    title: The column areas move every table down; the story does not weigh that
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): the area is the existing gap between the control and the table, from the
      first pixel after the control up to and including the table's border pixel (2 px plus
      table.border-width); the layout does not change, and the token for the area's size is gone.
      Widened only if practice asks for it; the context menu selects rows and columns meanwhile
      (decision 6).
  - id: O9
    category: failure
    severity: minor
    title: The row areas can lie outside the editor when its padding is small
    disposition: resolved
    disposition_rationale: >-
      With the area inside the existing gap (O8) it lies against the table and is visible wherever
      the table is; the story adds no space to the left. Whether the delete-row control is cut off
      by a small editor padding is a question about S018, not this story.
  - id: O10
    category: operational
    severity: minor
    title: What a screen reader gets for selected cells is not said (S009)
    disposition: fixed
    disposition_rationale: >-
      The selection is held in the table view's selection model, so that Qt's accessible table
      interface reports it; a scenario and S009's test cover it (decision 13; Armin, 2026-11-21).
  - id: O11
    category: failure
    severity: minor
    title: The "text the cell shows" of the tab-separated copy is not defined
    disposition: fixed
    disposition_rationale: >-
      The text of a field is the cell text the find bar searches (S028, "Table cells"): inline
      elements concealed, \| as |, <br> as a line break, entities as written (S041, decision 4;
      Armin, 2026-11-21).
  - id: O12
    category: cost
    severity: minor
    title: Size L for a story that also changes copy and paste for the whole editor
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): split in two stories. S040 keeps selecting, showing, deleting, typing and
      the selection in the editor; S041 (Order 46, M) takes the clipboard formats, copy, cut and
      paste.
---

# Review (spec) of S040 — Selecting cells, rows and columns in a table

## O1 - failure - major

In hitokoto the caret is always in a cell's text (S018: "there is no separate editing mode"), so
decision 4 makes `Shift+Space` select the row while the writer is typing. In Excel and Calc the
row key acts on a cell that is not being edited; while a cell is edited, the keys go to its text.
Typing with `Shift` held a moment too long (capitals, `:`, `?`, `>` followed by a space) then
selects the row, and with decision 12 the next character replaces the text of the end cell, a cell
other than the one being typed in. This is a known trap: OpenOffice issue 17209 describes the same
key in its data source view ("while typing quickly, one's finger sometimes still presses shift
after entering a symbol character") and reports the loss of the cell's content. Undo restores it,
but only if the writer notices the change in another cell. The story accepts that `Shift+Space` no
longer types a space; it does not weigh the combination with typing over a selection.

## O2 - design - major

Decision 11 makes `Ctrl+V` take `text/markdown` whenever the clipboard holds it, from any program.
Qt's own data for a rich-text selection carries `text/markdown` made by `QTextDocument::toMarkdown`
(the formats `OwnCopy` in `src/ui/editor_widget.cpp` names after `QTextEditMimeData`), and that is
not the text that was selected: Armin's sample of 2026-11-21 (quoted in S041) shows a blank line
between every line of a copied table. Any Qt program with a `QTextEdit` (and others that offer
Markdown) would paste into hitokoto differently from today:
escapes and blank lines appear that the plain text did not have. S031 kept such pastes as plain
text on purpose ("Qt is handed that text alone"). The rule is sound for hitokoto's own copies,
which carry the marker `application/x-hitokoto-copy`; for other programs the story gives no reason
to trust their Markdown over their text.

## O3 - premise - minor

Decision 1 fixes the start cell and moves the end cell; decision 2 frames the end cell, decision 9
puts the caret there and decision 12 types into it, "as in Excel". In Excel the active cell stays
where the selection started: "If you extend this selection in any direction, you'll see that the
active cell doesn't change. Only the selection changes." (Exceljet, "Shortcuts to move the active
cell"); Calc does the same (LibreOffice bug 39438). The story's draft scenario asked only that the
end cell be distinguishable. Either the frame and the typing target follow Excel (the start cell),
or the story says that hitokoto differs on purpose, so that "as in Excel" does not promise it.

## O4 - failure - minor

Decision 10 says "Line endings on the clipboard are LF". On Windows the plain text Qt puts on the
clipboard has CRLF: Armin's sample of 2026-11-21 shows `UnicodeText` with 6 lines and 5 CR, while
the `text/markdown` stream of the same copy has LF only. A test that compares the plain text with LF
passes on Linux and macOS and fails on Windows, or the implementation fights Qt's conversion. The
story should state LF for `text/markdown` and the platform's convention for `text/plain`, and the
tests should compare after normalising line endings.

## O5 - failure - minor

Decision 9 ends a selection of cells on "an edit through the find bar"; S028 is find only and edits
nothing. What does change a table's lines while cells are selected is not listed: undo and redo
(named), but also the add and delete controls of S018 used with the mouse while cells are selected,
`Ctrl+Shift+M`, and a table that becomes stale because a line it held was removed (S018,
"Robustness"). Each needs a defined end of the selection, or the view keeps indexes of a model that
was reset.

## O6 - failure - minor

Decision 12 adds rows and columns "as needed" when a pasted table does not fit. A table has at most
255 columns (S018, "Implementation": "up to 255 columns"; `unrefined.md`, "Tables with more than 255
columns"). A paste that would go past the limit is not defined: cut off, refused, or pasted as text
as S031 does for nested tables.

## O7 - failure - minor

Decision 12 says `Delete Row` and `Delete Column` remove all selected whole rows or columns and that
"the header row and the last column stay, as in S018". With every column selected, S018's rule (the
last column is never deleted) does not say which of them stays; with only the header row selected,
`Delete Row` does nothing, and the story does not say whether it is disabled. The context menu and
the keys of S018 should behave the same in both cases, and a test should pin it.

## O8 - design - minor

Decision 6 puts the column areas above the `−` controls, so the space above every table grows by
`table.select-area-size`. The table widget's height is what the header line reserves
(`TableEmbedder` sets the reserved height to `table.widget->height()`, and `TableWidget::fit` makes
it the view's height plus twice the gutter). Every table in every document moves down and the
paragraph above it moves away, even for a writer who never selects a column; S018's spacing below
a table ("at the distance of the add-row bar's space") stays as it was, so tables sit off-centre
between their neighbours. The alternative, an area that overlaps the existing gap or the top edge of
the header row, is not weighed.

## O9 - failure - minor

The row areas grow the space on the left, and the widget is placed at the editor's document margin
minus its gutter (`TableEmbedder::reposition`, `margin + offset.x() - gutter()`).
`layout.editor-padding` may be 0 to 256 (`docs/theming.md`); with a small padding the area, and
already today part of the `−` control, lies left of the viewport and cannot be reached. The story
should say what happens then (the table moves right, the area shrinks, or the areas are not
offered).

## O10 - operational - minor

S009 is the standing gate for caret, input methods and accessibility, and S018 added a table
scenario to it ("caret, input method and screen reader in cells"). S040 changes what is selected in
a table but says nothing about what a screen reader announces for a selection of cells, a row or a
column, or for a table covered by a selection in the editor. `QTableView` reports a selection
through its accessible table interface only if the view's selection model holds it; a selection
kept elsewhere and only painted would be invisible to assistive technology.

## O11 - failure - minor

Decision 10 fills the tab-separated copy with "the text the cell shows: no formatting, `\|` as `|`,
`<br>` as a line break". A cell can hold a link (its text or its destination?), inline code (with or
without backticks), escapes (`\*`), entities (`&amp;`) and raw HTML, which S018 shows as its
characters. S028 had to define the same thing for find (`concealedCellText`, with its own map) and
found that `tableCellToText` changes lengths. Without naming the function or listing the cases, two
implementations give different spreadsheet text and the test has nothing to check against.

## O12 - cost - minor

The story is sized L ("about a week of part-time work"). Beside selecting and painting cells it
changes hitokoto's copy for the whole editor (no HTML, no ODF), the paste of every program's
Markdown (S031), the context menu, delete, cut, paste and typing over cells, the editor's selection
across tables, two drawn pointers with SVG assets, eight theme tokens and the layout of every
table. S031 alone (M) needed several reworks after its merge. CLAUDE.md lets one branch hold closely
related stories; the copy and paste part (decisions 10 and 11 and the paste of decision 12) could be
a story of its own, reviewed and merged first, so that selecting does not wait for it.

## Capped concerns

- Plain text with tabs in one row (`a<TAB>b`) is no table under S031 and would be pasted into each
  selected cell with its tab, where a spreadsheet splits it over the columns.
- `Shift+Right` in an empty cell: the first press selects the cell as a cell (there is no text to
  select); the story implies it but no scenario shows it.
- Copying very large selections (the 1,000-row table) builds two Markdown strings and the shown
  text on demand; the budget only names S031's paste times.

## Explicitly not objecting to

- Rectangles only, no separate rectangles: Armin's decision, and Markdown tables have no use for
  them.
- Markdown as the main copy format and tab-separated text on request: checked with Armin's
  clipboard samples and Obsidian, which reads HTML or text but has no conversion of its own for
  tab-separated text.
- Dropping `text/html` and ODF from hitokoto's own copy: they are Qt's rendering of plain
  paragraphs, they broke the paste into Obsidian, and hitokoto writes no HTML of its own (brief,
  section 2).
- The pointers as SVG drawn through Qt SVG: hitokoto links `Qt6::Svg` already (application icon),
  so no new module and no image format plugin in the MSI (S037).
- Keeping `Ctrl+Space` and `Control+Space` although systems take them: Excel has the same overlap,
  and the mouse and the context menu remain.
- Emptying cells on `Delete` and on `Cut` at once: what Calc does, and a delayed move as in Excel
  does not fit a text editor whose file is the only copy (ADR 0002).
