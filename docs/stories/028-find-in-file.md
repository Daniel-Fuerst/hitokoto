# S028 — Find in the open file

- **Status:** In progress (confirmed by Armin on 2026-11-07, after review
  `docs/reviews/028-find-in-file-spec.md`) ·
  **Order:** 29 · **Size:** L ·
  **Depends on:** S013–S015, S018, S019, S020, S022, S023, S024, S026 · **Decision:** ADR 0008
- **Basis:** Armin's feature request and decisions of 2026-11-07. Search is listed in the brief
  under "Planned later" (section 1); this story is the scope extension that brings it in
  (CLAUDE.md: scope changes become new stories). The find bar follows Firefox's: at the bottom, not
  modal, searching while you type. Review decisions (Armin, 2026-11-07): O8 (when an element returns
  from its source), hard edges around link destinations, tables and properties, platform key
  bindings (O10), an announcement of the current match for screen readers (O11).

## Story

As a writer, I want to find text in the open file the way I read it, without the Markdown syntax
getting in the way, so that `wichtig: denken` finds `**wichtig:** denken` and I can step through
every place it occurs without the document changing.

## Decisions (Armin, 2026-11-07)

1. **The concealed view is searched**, wherever the caret is: syntax that hitokoto conceals while the
   caret is elsewhere does not count, also on the line the caret is on. `wichtig: denken` finds
   `**wichtig:** denken`.
2. **Link destinations are searched too.** A match in the destination of an inline link (S022)
   highlights the link text.
3. **Tables and front matter belong to this story.** A table is searched against the text of its
   cells, front matter shown as properties (S024) against its keys and values, without syntax
   (pipes, the delimiter row and YAML characters do not count). A match there shows the element as
   its source and highlights the match in it.
4. **Whitespace runs.** A run of one or more whitespace characters in the search text matches any run
   of one or more whitespace characters in the text (space, tab and every other Unicode whitespace
   character), also across line breaks, blank lines and block boundaries. Review: not across the
   edges of a link destination, a table or a properties block ("hard edges" below).
5. **Whitespace at the start or end of the search text counts:** it requires whitespace or a line or
   document boundary there. ` Agent` finds `Agenten` at the start of a word and not `Subagent`.
6. **No regular expressions.** A toggle "Match case" (`Aa`), off by default, kept for the session.
   Without it, case is folded the Unicode way: `ä` finds `Ä`, but `ä` does not find `a`. Review O10:
   its shortcut is `Alt+C` on Windows and Linux, `Cmd+Option+C` on macOS.
7. **Ctrl+F** (`QKeySequence::Find`, `Cmd+F` on macOS), also as Edit › Find, opens the find bar, puts
   the focus in the search field and selects the previous search text; with the bar open it brings
   the focus back to the search field.
8. **Find bar at the bottom, not modal:** searching while typing; `Enter` next match, `Shift+Enter`
   previous match, wrapping at the end of the document; a count `n of m`; `Escape` closes. A click
   in the text (on a link, say) acts on the first click, and the bar stays open. Review O10: next
   and previous are also `QKeySequence::FindNext` and `FindPrevious` (`F3`/`Shift+F3` on Windows
   and Linux, `Cmd+G`/`Shift+Cmd+G` on macOS).
9. **The caret moves only on closing.** Typing and `Enter`/`F3` only scroll to the match and
   highlight it; its line stays concealed. `Escape` closes the bar and selects the current match. A
   click in the text sets the caret as usual.
10. **Highlight all** is part of this story, as a toggle, off by default: on, every match in the
    visible area is highlighted in a paler colour, the current one strong yellow. The colours are
    theme tokens with fallbacks (ADR 0004); the dark theme has its own values.

Also: the search never blocks the UI (slices, also for matches across slice boundaries); a key
press takes at most 50 ms, also in a 10 MB file; benchmarks `s028_*`; the search never changes the
document (ADR 0002).

## Rules

**Units.** The searched text is a sequence of units in document order:

- **a line** outside tables and front matter shown as properties: its text with the hidden ranges
  of its `LineRender` removed (`ElementRegistry::renderDocument`, ADR 0006);
- **a link destination** (S022), placed directly after the line or cell that holds the link;
- **a table cell**, row by row, left to right;
- **a key, a value, a list or tags item** of front matter shown as properties, in document order.

Between two units lies a boundary. A **soft** boundary (between lines, between cells of one table,
between keys, values and items of one block) counts as a whitespace run for decision 4 and as a line
boundary for decision 5. A **hard** boundary (before the first and after the last cell of a table,
before the first and after the last unit of a properties block) counts as a line boundary for
decision 5, and no match crosses it. Two adjacent whitespace runs separated by a soft boundary are
one run. A link destination is a side unit with hard edges: no match enters or leaves it, and the
text around its link goes on past it as if it were not there (`hier` at the end of a line with a
link and `weiter` on the next line are found by `hier weiter`).

**Concealed text of a line.** What the hidden ranges remove: heading markers (`#` and the space
after them), bullet markers, emphasis and strikethrough delimiters, backticks of code spans, the
backslash of an escape, link brackets with destination and title, fences of code blocks (the
language badge is drawn, not text), thematic breaks and setext underlines. Everything else counts as
written, including text hitokoto does not render yet (ordered list markers, `>`, entities such as
`&auml;`) and the content of code blocks and code spans.

**Link destinations** are searched as `LinkTarget::destination` gives them: backslash escapes
resolved, entities as written (hitokoto decodes entities nowhere yet), no percent-encoding. For
ordering, for "at or after the caret" and for `F3`, a destination match stands at the start of its
link text. Its highlight covers the whole link text; `Escape` selects the link text. A link title is
not searched (it is a tooltip, S022).

**Table cells** (S018). The text of a cell is what a new core function gives (working name
`concealedCellText`): the cell's Markdown with `\|` read as `|` and `<br>` (any spelling) as a line
break, as `tableCellToText` does, then only the inline elements (S013–S015, S020, S022) run on it,
never the line elements (a cell `- x` keeps its `-`, `# x` its `#`), with a map from every code unit
back to its column in the line. Link destinations inside a cell are units (see above). Pipes, the
delimiter row, cells beyond the header's column count and text after the last pipe do not count. A
table shown as its source (`Ctrl+Shift+M`) is searched the same way. Known difference: the table
view draws a cell with Qt's Markdown (`setMarkdown`), which decodes entities; the search, like the
rest of hitokoto, takes `&auml;` as written.

**Front matter.** Closed front matter whose YAML `parseProperties` reads without error, with
`Options::frontMatterAsProperties` on, is searched as properties, whatever is on screen (caret in the
block, rows still being built, or the YAML shown for a match). Otherwise it is searched as lines of
text, YAML characters included. As properties, the units are each key, each value and each item of
a list or tags property, as the core reads them (`Property::key`, `text`, `items`): quotes, escapes
and folding resolved; numbers, checkboxes, dates and dates with time as written in the YAML
(`2026-11-07`, `true`), not as the system formats them. An unsupported value (shown as JSON) gives
one unit per key and per scalar inside it, without the JSON punctuation (quotes, colons, commas,
brackets). The title "Properties", comments, `---` delimiters, colons, indentation, `-` of block
sequences and the brackets and commas of flow sequences do not count. A core function maps every
code unit of a unit back to its source range in the YAML: an escape (`\t`, `ä`, `''`) maps to
its whole sequence, a folded line break to the break and the indentation after it. A match that
covers part of such a sequence covers all of it in the source.

**Whitespace** is every code point with the Unicode property `White_Space` (U+0009–U+000D, U+0020,
U+0085, U+00A0, U+1680, U+2000–U+200A, U+2028, U+2029, U+202F, U+205F, U+3000). Zero-width
characters (U+200B, U+FEFF) are not whitespace.

**Search text.** Inner whitespace runs are reduced to one run (decision 4). Leading and trailing
whitespace is a condition, not part of the match (decision 5): the match and its highlight start at
the first and end at the last non-whitespace character of the search text. An empty search text, or
one of only whitespace, finds nothing.

**Case** (decision 6, ADR 0008). Without "Match case", both sides are compared after simple Unicode
case folding (one code point to one, `CaseFolding.txt` status C and S): `Ä`/`ä`, `Σ`/`σ`/`ς`,
`ẞ`/`ß`. Full folding (`ß` = `ss`), diacritic folding (`ä` = `a`) and normalisation (`ä` precomposed
against `a` + U+0308) are not applied. With "Match case", code points compare as they are.

**Overlapping matches** do not exist: after a match the search continues behind it (`aa` in `aaaa`
is two matches).

**Where the search starts.** The first search after opening the bar starts at the caret (with the
focus in a cell editor: at the start of that cell): the current match is the first one that starts
at or after it, else the first in the document. While typing, the current match stays where it is
if it still matches with the new text, else the next match after its start becomes current
(Firefox). Next and previous step from the current match.

**Source view of tables and properties while the bar is open** (review O8, Armin). A table or
properties block shows its source exactly while it holds the current match. When the current match
leaves it (next or previous, typing in the field, an edit), it shows its cells or rows again at
once. "Highlight all" never switches an element. `Escape` with the current match in such an element
selects the match in its source, and the element keeps its source until the caret and the anchor
have both left it, as a table does today while a typed row is held; then the existing rules of S018
and S024 apply. Closing without a match, or with the current match elsewhere, returns the element
at once. If the focus is in a cell editor of a table that switches to its source, the focus goes to
the find bar if the step came from there, otherwise to the editor at the start of that cell's line.

## Acceptance scenarios

**Scenario: Ctrl+F opens the find bar**
- Given an open document
- When I press `Ctrl+F` (`Cmd+F` on macOS) or choose Edit › Find
- Then a find bar appears below the editor with a search field, "previous" and "next" buttons, the
  toggles "Match case" (`Aa`) and "Highlight all", the count and a close button
- And the focus is in the search field and its previous text is selected
- And the search with that text runs at once
- And when I press `Ctrl+F` again while the focus is elsewhere in the window (the editor, a table
  cell), the focus returns to the search field and its text is selected
- And with the bar closed, `F3` (`Cmd+G`) opens it as `Ctrl+F` does

**Scenario: searching while typing**
- Given the document `**wichtig:** denken` and the caret elsewhere
- When I type `wichtig: denken` into the search field
- Then after every key the current match is highlighted in `find.current-background` and scrolled
  into view
- And the count shows `1 of 1`
- And the line stays concealed, the caret and the selection in the editor do not move
- And when the whole document has been searched without a match, the count shows `No matches` and
  the field has `find.no-match-background`

**Scenario: stepping through matches**
- Given several matches
- When I press `Enter` in the field, `F3` (`Cmd+G` on macOS) or the "next" button
- Then the next match becomes current, is scrolled into view and the count shows its number
- And after the last match the first one follows (wrapping); `Shift+Enter`, `Shift+F3`
  (`Shift+Cmd+G`) or the "previous" button go back, before the first comes the last
- And `F3`/`Shift+F3` (`Cmd+G`/`Shift+Cmd+G`) also work while the focus is in the editor or a table
  cell and the bar is open

**Scenario: concealed syntax does not count**
- Given `# Titel`, `- Punkt`, `` `code` ``, `\*`, `~~weg~~`, `[Text](https://x.at)` and a fenced code
  block with info string `python`
- Then `Titel`, `Punkt`, `code`, `*`, `weg` and `Text` are found; `# Titel`, `- Punkt`, `` `code` ``,
  `\*`, `~~` and `python` are not
- And the content of the code block is found
- And the same holds when the caret is on that line (decision 1)

**Scenario: link destinations**
- Given `Absatz [Doku](https://example.com/handbuch) hier`
- When I search `handbuch`
- Then the match is counted, the link text `Doku` is highlighted, and stepping puts it before a
  match in `hier`
- And `hier https` and `Doku https` find nothing (hard edges)
- And `Escape` selects `Doku`

**Scenario: whitespace runs**
- Given `foo` at the end of a line, a blank line, and `bar` at the start of the next paragraph, and
  elsewhere `foo\tbar` and `foo` U+00A0 `bar`
- When I search `foo bar`
- Then all three are found
- And `foo  bar` (two spaces) in the search text finds the same

**Scenario: whitespace at the start or end**
- Given `Agenten`, `Subagent` and a line starting with `Agent`
- When I search ` Agent` (leading space)
- Then `Agent` in `Agenten` (after a space) and at the line start are found and highlighted without
  the space; `Subagent` is not
- And `Agent ` (trailing space) finds `Agent` before a space or at a line end, not in `Agenten`

**Scenario: match case**
- Given `Äpfel` and `äpfel`
- When I search `äpfel` with "Match case" off
- Then both are found, and `apfel` finds neither
- When I press `Alt+C` (`Cmd+Option+C` on macOS) in the find bar or click `Aa`
- Then "Match case" is on, only `äpfel` is found, and the toggle stays on until the app quits
- And typing `ç` (`Option+C` on macOS) into the field types `ç`

**Scenario: a match in a table**
- Given a table with a cell `**rot** und grün`, the caret above the table
- When I search `rot und`
- Then the table shows its Markdown lines (as with `Ctrl+Shift+M`) and the match is highlighted in
  them, on `rot`, ` und` and the concealed-in-the-cell-view `**` between them
- And pipes and the delimiter row are never part of a match: `a | b` does not find two cells; `a b`
  finds the end of one cell and the start of the next (soft boundary)
- And a cell `- x` is found by `- x`
- When I press `F3` and the next match is outside the table
- Then the table shows its cells again at once
- When I step back into the table and press `Escape`
- Then the match is selected in the table's lines and they stay shown until the caret and the anchor
  have left them

**Scenario: a match in front matter**
- Given front matter shown as properties with `title: "Hallo\tWelt"`, `date: 2026-11-07`,
  `tags: [rot, grün]` and `map: {a: 1}`
- When I search `Hallo Welt`, `2026-11-07`, `grün` or `a 1`
- Then the block shows its YAML and the match is highlighted there; for `Hallo Welt` the highlight
  covers `Hallo\tWelt` in the YAML
- And `title: "Hallo`, `"a":1` and `Properties` are not found, also while the caret is in the block
- And when the current match leaves the block, it shows its properties again

**Scenario: highlight all**
- Given several matches on the screen, one of them in a table shown as cells and one in a property
  value shown as a row
- When I turn on "Highlight all" (`Alt+A`, `Cmd+Option+A` on macOS, or the toggle)
- Then every match in the visible area of the text is highlighted in `find.match-background` and the
  current one in `find.current-background`
- And a cell, a property key or value, or a chip that contains a match, while its element shows
  cells or rows, gets `find.match-background` as its background (its characters are not marked;
  code review O3: the part that holds the match, not the whole row)
- And no element switches to its source for this, and no line moves
- And the toggle is off by default and kept for the session

**Scenario: escape closes and selects**
- Given a current match and the focus in the find bar
- When I press `Escape` (or click the close button)
- Then the bar closes, the focus returns to the editor and the match is selected (its source range,
  so its line shows its syntax); for a destination match the link text
- And without a match the caret stays where it was
- And no highlight remains
- And `Escape` with the focus in the editor or a table cell keeps its existing meaning there and does
  not close the bar

**Scenario: clicks in the text**
- Given the open bar with a current match
- When I click a link in the text
- Then the link opens on that first click (S022), and the bar stays open
- When I click elsewhere in the text
- Then the caret is placed there as usual (S010), the bar stays open and `F3` continues from the
  current match

**Scenario: editing while the bar is open**
- Given the open bar and matches
- When I type in the document, including a fence that turns the rest into code, a delimiter row
  that makes a table, and a change to the first line that ends front matter
- Then the matches and the count follow the edit without blocking typing, equal to a fresh search
  of the edited document once settled
- And the current match stays current while it still matches

**Scenario: the search never changes the document**
- Given any document and any search, including matches in tables and front matter
- Then the text, the undo stack and the modified state are unchanged (ADR 0002)
- And saving afterwards writes the same bytes as before

**Scenario: the count**
- Given a document being searched
- Then until the whole document has been searched the count shows `m+ matches` (the matches found so
  far, no index)
- And once it has, it shows `n of m`, or `No matches`

**Scenario: large files stay responsive**
- Given a 10 MB document
- When I type into the search field, step with `F3` or type in the document with "Highlight all" on
- Then no key press takes longer than 50 ms, and the 95th percentile of keys in the search field stays
  within one frame (16 ms)
- And matches across slice boundaries are found
- And a search text of one letter (hundreds of thousands of matches) keeps these budgets

**Scenario: screen reader** (review O11)
- Then the search field, buttons and toggles have accessible names ("Find", "Previous match", "Next
  match", "Match case", "Highlight all", "Close find bar"); the toggles report their state
- And each time the current match changes, an announcement (`QAccessibleAnnouncementEvent`, Qt 6.8)
  reads the count and the concealed text around the match, e.g. `3 of 12: … Agenten arbeiten …`
  (at most about 80 characters)

**Scenario: find bar is themeable (theming baseline, ADR 0004)**
- Given a theme that sets every `find.*` token (table below)
- Then the bar, its field, buttons, toggles, count and the highlights use them
- And a theme that sets none of them falls back as documented in `docs/theming.md`
- And in the built-in light and dark themes the highlight text has a contrast of at least 4.5:1
  against its highlight background

**Scenario: concealment quality gate (S009)**
- Given matches whose source range covers concealed delimiters (`**`, a link's brackets), with
  "Highlight all" on
- Then the highlighted line shows no marker pixels (the highlight's text colour applies only to the
  visible parts of the match)
- And when the current match moves and "Highlight all" changes, no line moves or changes height; only
  a table or properties block switching to or from its source for the current match changes height,
  at that moment, as with `Ctrl+Shift+M`
- And after `Escape` into a table or properties block, the caret and selection are only ever on
  lines that are shown

## Tokens

`[find]`, documented in `docs/theming.md` like every element:

| Token | Type | Meaning | Fallback |
|---|---|---|---|
| `find.background` | color | Background of the find bar | `colors.surface` |
| `find.border` | color | Line between the editor and the bar | `colors.border` |
| `find.border-width` | number (px) | Width of that line, 0 to 8 | 1 |
| `find.padding` | number (px) | Space inside the bar, 0 to 32 | `layout.spacing` |
| `find.spacing` | number (px) | Space between the bar's controls, 0 to 32 | `layout.spacing` |
| `find.text` | color | Count and labels | `colors.text-muted` |
| `find.font-family` | string | Font of the field and count; "" = UI font | "" |
| `find.font-scale` | number | Size relative to the UI font, 0.5 to 2 | 1 |
| `find.field-background` | color | Search field | `colors.background` |
| `find.field-text` | color | Text in the search field | `colors.text` |
| `find.field-border` | color | Border of the search field | `colors.border` |
| `find.field-focus-border` | color | Border of the focused search field | `colors.primary` |
| `find.field-border-width` | number (px) | 0 to 8 | 1 |
| `find.field-radius` | number (px) | 0 to 32 | `layout.radius` |
| `find.no-match-background` | color | Search field when nothing matches | `#f8d7da` (light), `#5c2b2e` (dark) |
| `find.icon` | color | Icons of buttons and toggles | `colors.text` |
| `find.button-size` | number (px) | Width and height of buttons and toggles, 16 to 64 | 24 |
| `find.button-radius` | number (px) | 0 to 32 | `layout.radius` |
| `find.hover-background` | color | Button or toggle under the mouse | `colors.border` |
| `find.active-background` | color | Pressed button, toggle that is on | `colors.selection` |
| `find.active-icon` | color | Icon of a toggle that is on | `colors.selection-text` |
| `find.current-background` | color | Current match | `#ffd43b` (light), `#c99a06` (dark) |
| `find.current-text` | color | Text of the current match | `#1a1a1a` |
| `find.match-background` | color | Other matches, cells and values with a match | `#fff3bf` (light), `#5c4a12` (dark) |
| `find.match-text` | color | Text of other matches | `#1a1a1a` (light), `#f1f1f1` (dark) |

"(light)"/"(dark)" are the fallbacks of the built-in light and dark themes (`base`). The fixed text
fallbacks keep the contrast whatever a theme sets for `colors.text`.

## Requirements for the implementation

- **Core.** The matcher lives in the core (CLAUDE.md): compile the search text; walk the units of a
  range of lines with their source offsets; report matches as source ranges. New core functions for
  the concealed text of a table cell with its map (`concealedCellText`) and for the source map of
  front-matter units (beside `parseProperties`). Case folding from a table generated from Unicode's
  `CaseFolding.txt` (ADR 0008). Core tests `S028_FindInFile` for every rule above, the units checked
  against `renderDocument` and against random documents.
- **Edits and caching** (review O4). Matches are kept as a count per block (with prefix sums for the
  index `n`), positions only for the visible blocks and the current match. An edit invalidates from
  its first block down only as long as the block state entering a block differs from the cached one
  (the styler's rule; the search reuses the styler's cached states where they are complete).
  A search in progress restarts at the edited block, not at the document start. Front matter
  confirmed or ended and a table's extent changing are explicit triggers. A UI test compares the
  matches after random edits with a fresh search.
- **Two passes** (review O6): one from the caret forward for the current match, one from the start
  for the count; both in slices of at most 6 ms in idle time, the work on a key bounded as for the
  styler.
- **Properties** are not parsed again per key: the search uses the `Properties` the embedder holds
  (S024 code review O3), or parses once per change of the front matter.
- **Painting.** Highlights are painted in the decorations pass or as extra format ranges that never
  touch the document; the text colour only on the visible parts of a match. Highlights never change
  geometry. Cells, values and chips get their background through the table delegate and
  `PropertiesView`.
- **Keys.** Next and previous are `QKeySequence::FindNext`/`FindPrevious` plus `F3`/`Shift+F3` where
  the platform binding does not include them, window-wide while the bar is open; `Enter` and
  `Shift+Enter` in the field. The toggles get mnemonics ("Match &case", "&Highlight all") on Windows
  and Linux and `Cmd+Option+C`/`Cmd+Option+A` on macOS, so no text key is taken from the field.
- **Benchmarks** `s028_*` in the 10 MB document: keys in the search field (key to paint: median,
  p95, max), time to the first match, time to the full count (informational: target under 1 s on
  the CI runner), `F3`, typing in the document with "Highlight all" on, and the time from the last
  key in the document until the count is complete again (`s028_typing_settled_ms`); all hard
  budgets hold.
- **S009** gets the scenario above and `tst_s009_concealment` a walk with highlights on.

## Tests

- Core: `S028_FindInFile` (units, boundaries, whitespace, case, cells, properties with escapes and
  unsupported values, destinations, overlap, random documents against `renderDocument`, tokens);
  added with the implementation: `ScanningInPiecesFindsWhatOneScanFinds` (the UI's pieces).
- UI: `tst_s028_find_in_file` (bar, keys per platform binding, stepping and wrapping, count, source
  view of tables and properties per review O8, `Escape`, clicks, edits against a fresh search,
  highlight all in text and cells, highlights without marker pixels, no document change,
  accessibility names and announcements, tokens, case folding against `QChar::toCaseFolded`).
- S009: `tst_s009_concealment::findHighlightsWhileWalking` and
  `caretOnShownLinesAfterEscapeIntoATable`.
- Benchmarks: `s028_*` (keys in the field, first match, full count, next match, typing in the
  document with "Highlight all" on, settled).
- Added with the code review (`docs/reviews/028-find-in-file-code.md`):
  - Core: `ADestinationSkippedByAMatchAcrossALineEndIsStillSearched` (O1),
    `MatchContextLeavesOutSkippedDestinations` (O2),
    `ABuilderContinuedInATableBuildsItsRowsAsFromTheHeader` (O8).
  - UI: `anEditCompletesAMatchAcrossLinesWithoutConcealedText` (O4),
    `aDelimiterRowTypedOrDeletedMakesOrBreaksATable` (O5),
    `frontMatterClosedOrFixedByAnEditIsSearchedAsProperties` (O6),
    `frontMatterIsParsedOncePerChange` (O12), `thePassAheadOnlyPicksMatchesOfTheDocument` (O7),
    `aLongTableLooksUpMatchesFromTheirRow` (O8), `stepsBeforeTheCountIsCompleteAreQueued` (O9),
    `escapeIntoATableHoldsTheWholeTable` (O10), `findNextFromACellIntoItsOwnTable` (review O8,
    rule 4), `highlightAllMarksTheKeyTheValueOrTheChip` (O3),
    `aLinkWhoseTextIsItsDestinationGivesTwoMatchesAndAnnouncesBoth`, `noMatchesIsAnnounced`,
    `theBarsButtonsTakeTheFocusWithTab`; `aMatchInATableShowsItsSourceWhileCurrent` also checks
    that the table view is kept; `editsUpdateTheMatchesLikeAFreshSearch` now covers
    multi-word queries, tables, front matter and chained edits and compares the count, the current
    match, its number and the matches on screen with a fresh search (O11).
  - S009: geometry after every step and no hidden range under a highlight's text colour (O11).
  - Benchmarks: `s028_step_incomplete_*` (O9), `s028_table_field_typing_*` and
    `s028_table_step_*` on the 1,000-row table with "Highlight all" (O8).

## Implementation

On `agents/s028-find-in-file`, 2026-11-07:

- Core (`find.hpp`, `find.cpp`): `FindQuery`, `lineUnits`, `tableRowUnits` (only the inline
  elements run on a cell: `ElementRegistry::renderInline`), `propertyUnits` on
  `propertySourceUnits`, which the YAML parser now fills with a source map per key, value and item
  (escapes, `''` and folded breaks atomic), `FindUnitBuilder` for the boundaries, `scanUnits` that
  resumes where a match needs more units, `documentUnits` and `findInDocument` as the reference,
  `matchContext`. Case folding from `case_folding_table.inc`, generated from Unicode 18.0.0
  `CaseFolding.txt` (1,533 mappings, ADR 0008); it agrees with `QChar::toCaseFolded` of Qt 6.12
  for every character Qt knows.
- UI: `FindBar`, `FindController`, `EditorWidget::setFindHold` and `showPosition`,
  `TableEmbedder::setFindHold` (a table with the current match, and after `Escape` until the caret
  and the anchor leave it), `PropertiesEmbedder::setFindHold`, `TableModel::setFindCells`,
  `PropertiesView::setFindRows`; Edit › Find, Find Next and Find Previous in `MainWindow`.
- Tokens `[find]` in `docs/theming.md`.
- Two test fixes found while making the tests green: `nothingFoundMarksTheField` erased one letter
  and still had no match (now it clears the field); `editsUpdateTheMatchesLikeAFreshSearch` typed
  its pieces with `keyClicks`, which asserts on a line break (now `Key_Return` and `test::typeText`).

After the code review (`docs/reviews/028-find-in-file-code.md`, all twelve objections fixed):

- Core: `scanUnits` keeps the matches of destinations a match went past (O1); `matchContext`
  leaves them out (O2); `FindUnitBuilder::continueTable` (a lookup starts at a table row, O8) and
  `addPropertyUnits` (front matter parsed once, O12).
- `FindController`: front matter found and parsed once per change of it, with explicit triggers on
  edits (O6, O12); the walk up from an edit and the start of the pass ahead count only lines with
  concealed text (O4, O7), and an edit restarts at least on the line above it (O5); a match of the
  pass ahead that the pass from the start does not confirm is found again (O7); steps work the pass
  for at most 8 ms and wait in a queue (O9); an edit below where the pass reads goes on with that
  pass; "Highlight all" marks only what is on screen (O8) and the key, value or chip in the
  properties block (`PropertiesView::setFindMarks`, O3); `inDestination` tells two matches of one
  range apart; "No matches" is announced.
- `TableEmbedder::setFindHold` holds the whole table after `Escape` (O10); a table that shows its
  source only for the find bar keeps its view hidden and shows it again without building it (found
  with the new table benchmark: 88 ms for a key that moved the match out of a 1,000-row table);
  the bar's buttons take the focus with Tab.
- Not done, documented in the record: prefix sums for the index `n` (the counts per line are added
  up on refresh, well under a millisecond for 10 MB).
- One test fix: `frontMatterIsParsedOncePerChange` expected one match after typing `x` before
  `Hallo`, but `xHallo` still contains it; it now compares with a fresh search.
