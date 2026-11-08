---
story: S028
mode: spec
spec: docs/stories/028-find-in-file.md
date: 2026-11-07
dispositioned: 2026-11-07 (Armin decided O5 with the hard edges, O8, O10 and O11; the others and the capped concerns as proposed by Claude, before the red tests)
reviewer: Advocatus Diaboli (independent subagent)
objections:
  - id: O1
    category: design
    severity: major
    title: "The searched text of a table cell has no source in the core; renderDocument gives none and render() would be wrong"
    disposition: fixed
    disposition_rationale: "Story, rules 'Table cells': a new core function (working name concealedCellText) gives a cell's text with tableCellToText's substitutions, only the inline elements run on it (never line elements: '- x' keeps its '-'), with a map from every code unit to its column; it, not renderDocument, is the reference for cells. Link destinations in cells are units. The entity difference to the table view (setMarkdown decodes entities) is accepted and documented. Scenario 'a match in a table' covers '- x'."
  - id: O2
    category: design
    severity: major
    title: "Matches in property values cannot be mapped back to YAML source ranges (escapes, folding, JSON)"
    disposition: fixed
    disposition_rationale: "Story, rules 'Front matter': a core function beside parseProperties maps every code unit of a key, value or item to its source range; an escape maps to its whole sequence, a folded break to the break and the indentation; a match covering part of a sequence covers all of it. Unsupported values give one unit per key and per scalar inside, without JSON punctuation. Scenario with an escaped value (Hallo\tWelt) and an unsupported value (map: {a: 1})."
  - id: O3
    category: design
    severity: minor
    title: "Whether front matter is searched as units or as text depends on view state the search itself changes"
    disposition: fixed
    disposition_rationale: "Story: the mode comes from the core, not the view: closed front matter that parseProperties reads without error, with frontMatterAsProperties on, is always searched as properties (caret in the block, rows not built, YAML shown for a match). Scenario includes the caret in the block."
  - id: O4
    category: design
    severity: major
    title: "No rule for what an edit invalidates; a second block-state cache beside BlockStyler"
    disposition: fixed
    disposition_rationale: "Story, requirement 'Edits and caching': a match count per block with prefix sums, positions only for visible blocks and the current match; an edit invalidates from its block down only while the entering block state differs (styler's rule, reusing its complete states); a pass in progress restarts at the edited block; front matter and table extent changes are explicit triggers. Benchmark s028_typing_settled_ms; UI test comparing matches after random edits with a fresh search; editing scenario with a fence, a delimiter row and the first line."
  - id: O5
    category: design
    severity: minor
    title: "Link destination units: their place in the order, cross-unit matches and what Escape selects are undefined"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-07: hard edges around a link destination (also tables and properties blocks): no match crosses them. Story: a destination match stands at the start of its link text for ordering, the caret rule and F3; Escape selects the link text; scenario 'hier https' finds nothing. The story's claim that entities are resolved in destinations is corrected (entities as written, see capped concerns)."
  - id: O6
    category: design
    severity: minor
    title: "The count cannot show n while the search is incomplete, because the search starts at the caret"
    disposition: fixed
    disposition_rationale: "Story: two passes (from the caret for the current match, from the start for the count); until complete the count shows 'm+ matches' without an index, 'No matches' only after the whole document; time to the full count gets an informational target (under 1 s on the CI runner)."
  - id: O7
    category: design
    severity: minor
    title: "Highlight all inside tables and properties shown as cells or rows cannot highlight characters"
    disposition: fixed
    disposition_rationale: "Story, scenario 'highlight all': a cell, a property value or a chip with a match gets find.match-background as its background while its element shows cells or rows; characters are marked only in the source. No element switches for highlight all."
  - id: O8
    category: failure
    severity: major
    title: "When a table or properties block returns from its source is unspecified, and Escape can leave the selection on hidden lines (S009)"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-07: an element shows its source exactly while it holds the current match and returns at once when the current match leaves it; highlight all never switches. Escape with the match inside selects it in the source, and the element keeps its source until caret and anchor have left it (like the held state of a table); closing without a match there returns it at once. Focus from a cell editor goes to the bar or the editor at that cell's line. S009 story gets a scenario for find highlights and the caret after Escape."
  - id: O9
    category: failure
    severity: minor
    title: "A highlight that sets a text colour makes concealed markers visible"
    disposition: fixed
    disposition_rationale: "Story: the highlight's text colour applies only to the visible parts of a match; S009 scenario 'no marker pixels' with highlight all on (story and S009 story)."
  - id: O10
    category: failure
    severity: minor
    title: "macOS shortcuts: Alt+C and Alt+A swallow typed characters; F3 is a system key"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-07: platform bindings. Next/previous: Enter/Shift+Enter in the field, QKeySequence::FindNext/FindPrevious plus F3/Shift+F3 where not included (Cmd+G/Shift+Cmd+G on macOS); toggles as mnemonics Alt+C/Alt+A on Windows and Linux, Cmd+Option+C/Cmd+Option+A on macOS. Scenario: Option+C types ç in the field on macOS."
  - id: O11
    category: failure
    severity: minor
    title: "A screen reader never learns where the current match is, because the caret does not move"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-07: each change of the current match is announced with the count and the concealed text around it (about 80 characters) through QAccessibleAnnouncementEvent, available since Qt 6.8 (doc.qt.io, checked 2026-11-07), within the minimum version."
  - id: O12
    category: operational
    severity: minor
    title: "Token proposal is incomplete: misaligned fallbacks, missing values and states, contrast not ensured by the fallback"
    disposition: fixed
    disposition_rationale: "Story, section 'Tokens': one row per token in the form of theming.md with type, range and fallback; hover, pressed, focus, font and size tokens added; current-text and match-text fall back to fixed dark or light values, not colors.text; light and dark fallbacks of the built-in themes named; the contrast scenario is limited to the built-in themes."
---

# Review S028 — Find in the open file (spec)

Scope: the draft story `docs/stories/028-find-in-file.md` (lines cited as `story:N`), read against
CLAUDE.md, the brief (sections 1, 4, 5, 8), `docs/architecture.md`, ADRs 0002–0006,
`docs/stories/README.md`, S009, `docs/theming.md`, `docs/performance.md` and the code in `src/core`
and `src/ui`. Core behaviour was checked with probes built outside the repo
(`g++ -std=c++20 -O2 -Isrc/core/include … element_registry.cpp elements/*.cpp block_rules.cpp
properties.cpp text_codec.cpp reveal.cpp`, in `/tmp/claude-0/s028-review`). Decisions 1–10 are
taken as given. Objections against them are raised only where the evidence shows a conflict.

## O1 - design - major

**Objection.** The story defines a cell's searched text as "its Markdown as the core reads it
(`tableRowCells`, `tableCellToText` …), with the hidden ranges of the inline elements … removed"
(story:74–76). It also requires that "the concealed text must equal the one `renderDocument` gives"
(story:264). No core function produces this text, and `renderDocument` gives nothing for cells.

**Evidence.**
- Probe: `renderDocument` on a valid table (``| a | **rot** und grün | `x` |``, delimiter row, a row
  `| c \| d | e<br>f | [L](u) | extra |`) returns `table=1` with `hidden=0`, `inlineStyles=0` and
  `links=0` for every table line. The only output is `cells` with raw content spans
  (`{**rot** und grün}`, `{[L](u)}`). Inline elements do not run inside multi-line blocks
  (architecture.md, "Adding a Markdown element": "While a block is open it owns each line; line and
  inline elements do not run there"). So the reference in story:264 contradicts story:74–76 for
  exactly the element that decision 3 adds.
- `ElementRegistry::runInlineElements` is private (`markdown.hpp`, private section;
  `element_registry.cpp:380`). The only public route is `render(cellText, {})`, which also runs the
  line elements. The probe shows `- Punkt` → ` Punkt` and `# Titel` → `Titel`, so a cell `- x` or
  `# x` would lose characters that GFM keeps (cells hold inline content only).
- `tableCellToText` changes lengths: `c \| d` (6 code units) becomes `c | d` (5), and `e<br>f`
  becomes `e` + LF + `f` (probe). There is no offset map back to the source, yet the matcher must
  "report matches as source ranges … per unit for … cells" (story:261–263), and the table scenario
  needs a highlight over `rot** und` (story:176).
- The rules do not say whether link destinations inside cells are units. The unit list names
  "each link destination (after the link's line)" (story:63–64), but table lines carry no `links`
  (probe).
- What the user reads in a cell is what `TableDelegate::prepare` draws:
  `QTextDocument::setMarkdown(…, MarkdownDialectGitHub)` (`src/ui/table_view.cpp:134`). md4c resolves
  entities and hides raw HTML there, but the story counts `&auml;` "as written" (story:60). A cell
  that shows `ä` is therefore not found by `ä`. That goes against the story's own premise
  ("the way I read it", story:12).

**What would resolve it.** Add a core function to the requirements, for example
`concealedCellText(line, TableCell) -> {text, sourceOffsets, links}`. It applies the
`tableCellToText` substitutions, runs only the inline elements on the result, and keeps a code-unit
map to line columns. Name this function, not `renderDocument`, as the reference for table units.
State whether destinations in cells are units. State which display defines "as hitokoto shows it"
for cells, the core's concealment or Qt's delegate, and accept the entity difference explicitly if
it stays.

## O2 - design - major

**Objection.** The story requires that a match in a property "shows the block as its YAML and the
match is highlighted there" (story:184) and that Escape selects "its source range" (story:199–200).
The value text is searched as `Property::text`/`Property::items` (story:81–84). `Property` carries no
source positions per character, item or escape, and for several value kinds a match has no
counterpart in the YAML at all.

**Evidence.**
- `properties.hpp`: a `Property` has `key`, `text`, `items`, `keyLine`, `valueLine` and
  `valueColumn`. There are no spans for the items and no map from text offsets to source.
- Probe (`parseProperties`):
  - `title: "Hallo\tWelt ä"` gives text `Hallo` TAB `Welt ä`. One code unit `ä` comes from
    six source characters, and the whitespace TAB from the two characters `\t`.
  - `folded: >` followed by `  a` and `  b` gives `a b` LF. The space comes from a line break plus
    indentation.
  - `plain: a` continued by `  b c` gives `a b c`.
  - `'q k': 'it''s'` gives `it's`.
  - `map: {a: 1, b: [x]}` (unsupported) gives `{"a":1,"b":["x"]}`. Its quotes and the lack of
    spaces do not exist in the source. Searching `"a":1` finds a match that has no YAML range.
  - `tags: [x, "y z"]` gives items `[x][y z]` with no column per item.
- The story's own rule excludes YAML punctuation from matching (story:84–86). For JSON
  punctuation, which is synthesised, it sets no rule.

**What would resolve it.** Extend `parseProperties`, or add a companion function, so that it returns
a source map for each key, value and item: for each code unit of `text` or `items[i]`, the (line,
column) range it came from. An escape maps to its whole sequence and a folded break to the line
break plus indentation. Specify that a highlight covering part of an escape covers all of it. For
unsupported values, choose one of two rules. Either search the YAML source of the value with YAML
punctuation excluded, as for other values. Or search only the JSON scalars and keys (no quotes,
colons, commas or brackets) and map each one to its source token. Add a scenario with an escaped
value and one with an unsupported value.

## O3 - design - minor

**Objection.** The searched text of front matter changes with whether it is "shown as properties"
or "shows as text" (story:80–87). The story does not define that state for the cases where the YAML
shows although the front matter is valid. For tables the story settles this ("A table shown as its
source … is searched the same way", story:77–78). For front matter it does not.

**Evidence.** `PropertiesEmbedder::apply` shows the block only if
`enabled_ && close_ >= 0 && !error_ && !dirty_ && view_->built() && !caretInside()`
(`src/ui/properties_embedder.cpp:302–303`). So the YAML shows while the caret is inside, while the
rows are still being built ("until then the YAML shows", architecture.md, PropertiesEmbedder), and,
under decision 3, while a match is current there. If "shown" means what is on screen, then
`title: "Hallo` is found while the caret is in the front matter but not otherwise (contrast
story:185). Showing the YAML for a match would also change the searched text and the count it
produced. The scenario at story:181–187 cannot be tested deterministically without this rule.

**What would resolve it.** Define the mode from the core, not the view: closed front matter whose
`parseProperties` has no error, with `frontMatterAsProperties` on, is always searched as
keys/values/items, whatever is on screen (caret inside, rows not built, shown for a match). Add the
caret-inside case to the scenario.

## O4 - design - major

**Objection.** The story requires that "the search computes block states itself in its slices"
(story:265) and that "the matches and the count follow the edit, without blocking typing"
(story:212–216). It does not say what an edit invalidates, how a sliced pass survives edits between
slices, or how this cache relates to the one `BlockStyler` already keeps. That is a second
incremental implementation of the ADR 0006 state chain. ADR 0006 needs comparison tests to keep the
first one correct.

**Evidence.**
- `BlockStyler` already caches the block state per line, propagates "restyling only while the state
  changes", confirms front matter, and reports completion through `isComplete` (architecture.md,
  BlockStyler). ADR 0006 decision 1 required `tst_s023_thematic_breaks` to compare the lazy path
  with `renderDocument`. A parallel walk needs the same guarantees.
- One edit can change the searched text of everything below it. Typing a fence turns the rest into
  code, which removes all hidden ranges. Deleting a delimiter row ends a table. Editing the first
  line removes front matter (ADR 0006, "Implementation": front matter is confirmed again "when an
  edit touches the first line …").
- Cost on this VM, core only, `-O2`, the 10 MB `largeDocument` of `core_bench.cpp`
  (445,327 lines): `renderDocument` takes 184 ms and a `blockStateAfter` walk takes 32 ms (probe).
  The UI adds `QTextBlock::text()` copies and the matching. If every keystroke in the document
  restarts a full pass, then at about 6 ms per idle slice the count never settles while someone
  types steadily in a 10 MB file. The story still requires the count to follow the edit, with
  Highlight all on (story:225).
- `performance.md` shows the established pattern: work is bounded per key and continues in idle
  slices, with a "settled/restyled after" measurement (`s023_bullet_settled_ms`,
  `s023_underline_settled_ms`). The story names no such measurement for the search.

**What would resolve it.** State the model in the requirements:
1. Matches are kept per line or per unit and keyed so that untouched lines keep theirs.
2. An edit invalidates from its first line down, only as long as the entering block state differs.
   This is the styler's rule, and the walk can reuse the styler's cached states where they exist.
3. A pass in progress restarts at the edited line, not at the document start.
4. Front-matter confirmation and table extent changes are explicit invalidation triggers.

Add `s028_typing_settled_ms` (time from the last key until the count is complete again) to the
benchmark list. Also require a core or UI test that compares the search's units with the reference
after random edits, like the S023 comparison.

## O5 - design - minor

**Objection.** A link destination is a unit "after the link's line" (story:63–64), and boundaries
count as whitespace (story:65–67). Several things follow that the story does not address:
cross-unit matches that make no sense, stepping order that runs backwards, an undefined "at or
after the caret" for destination matches, and an undefined source range for Escape.

**Evidence.**
- Line `Absatz [Doku](https://example.com/handbuch) hier`: the line unit is `Absatz Doku hier`
  (probe), followed by the unit `https://example.com/handbuch`. By story:65–67 the search
  `hier https` matches across the line end and the destination. Its highlight would be `hier` plus
  the whole link text `Doku`, which lies to the left of it. Likewise `handbuch` plus the first word
  of the next line matches.
- F3 order follows the units. A match in `hier` is followed by the destination match, which
  highlights `Doku` further left (story:71). F3 then jumps backwards within the line.
- Story:105–106 make the first current match "the first one that starts at or after the caret". The
  story does not give the position of a destination match for that comparison.
- Escape "selects … its source range" (story:199–200). For a destination match it is open whether
  that is the destination inside `(…)` or the highlighted link text. The destination text is not a
  substring of the source when escapes are resolved: `[x](y\)z)` gives destination `y)z` (probe).
- Related, and checkable separately: story:70 says `LinkTarget::destination` has "escapes and
  entities resolved". The probe gives `https://example.com/hand&amp;buch` unchanged: entities are
  not resolved, and `src/core/src/elements/link.cpp` has no entity handling. CommonMark resolves
  them (markdown-it-py, mode commonmark: `[x](/f&ouml;)` gives href `/f%C3%B6`). See Capped
  concerns.

**What would resolve it.** For ordering and the caret rule, give a destination match the position
of its link text's start. Make the boundaries around a destination unit hard: no match spans into or
out of a destination, so decision 4 does not apply there. Make Escape select the link text, and
reveal the line so that the destination is visible. Add a scenario for `hier https` not matching.

## O6 - design - minor

**Objection.** This is one of the points the story leaves "open for the review" (story:227–228,
story:279). The proposed interim display, "the matches found so far", is underspecified. Because
the first search starts at the caret (story:105), the index `n` of the current match is unknown
until the part of the document before the caret has been searched. `n of m` therefore cannot be
shown truthfully during an incomplete pass, even when the current match is already known.

**Evidence.** Story:105–107 search forward from the caret and wrap. Story:227–228 require `n of m`
only when the whole document is searched. Decision 8 promises the count `n of m` (story:38). No
budget or measurement bounds how long the count stays incomplete (story:271–273 list "time to the
full count" without a budget). Firefox, which the story cites as its model (story:7–8), shows a
plain total ("More than 1000 matches") rather than a partial index.

**What would resolve it.** Search two ways at once: forward from the caret to find the current
match quickly, and from the document start for the count. Until the count pass is complete, show
`m+ matches` (or `at least m`) without an index, then `n of m`. Show `No matches` only after the
whole document has been searched. Give "time to the full count" in a 10 MB document a target, even
an informational one, so the gate can track it.

## O7 - design - minor

**Objection.** Decision 10 highlights "every match in the visible area" (story:44–46, story:189–194).
The story leaves open how this works in a table shown as cells or front matter shown as rows
(story:282). With today's widgets no character-level highlight is possible there.

**Evidence.**
- Cells are painted from a `QTextDocument` filled by `setMarkdown`, after `\|` and `<br>`
  substitutions (`src/ui/table_view.cpp:127–134`). Offsets in that rich-text document bear no
  relation to source columns or to O1's cell text.
- Property rows show dates as `QLocale::system().toString(date, QLocale::ShortFormat)`
  (`src/ui/properties_view.cpp:69–75`). A match on `2026-11-07` (story:83–84) has no characters in a
  row showing, say, `07.11.26`. A checkbox (`true`) has no characters at all.
- Decision 3 already admits this for the current match: it switches the element to its source. For
  Highlight all, switching every visible element is not acceptable, because story:246–247 allows
  only "a table or properties block switching to its source for a match" to change height.

**What would resolve it.** Specify unit-level marking for elements shown as widgets: a cell or a row
(key or value part, or a chip) gets `find.match-background` as a background when it contains a
match, and the matched characters are not marked. Character highlights appear only when the element
shows its source. Add this to the highlight-all scenario and the theming list.

## O8 - failure - major

**Objection.** Decision 3 shows a table or properties block "as its source" for a match. Decision 9
keeps the caret where it is. The story leaves open when the element shows its cells again
(story:179, story:277–278). It also does not say what Escape (story:196–202) does to that element.
Each possible reading breaks an existing invariant.

**Evidence.**
- Escape "selects its source range, so its line shows its syntax", and "no highlight remains"
  (story:199–202). Suppose the table returns to its view on close. Then the selection lies in lines
  that `BlockLayout` hides again, and `TableEmbedder::caretMoved` moves only "a collapsed caret that
  lands on hidden table lines" into the view (`table_embedder.hpp`, `caretMoved`). A selection is not
  collapsed. S009 requires that "the caret is only ever in a cell or on a visible line" (S009, table
  cells). Suppose instead the table stays in source mode. Then it behaves like `Ctrl+Shift+M` (the
  `source` flag in `TableEmbedder::Table`) and stays there until the user toggles it back, which the
  story does not intend.
- For front matter, S009 says "the YAML shows exactly while the caret is on a line of the front
  matter, the properties block exactly while it is not" (S009, front matter scenario). Story:184
  shows the YAML while the caret is elsewhere, and story:243–247 do not amend that S009 scenario.
- There is a case with the focus in a cell editor: Ctrl+F (story:119–120) and F3 work from there.
  If the next match is in the same table, the view must switch to source while its `CellEditor`
  has the focus. The story does not say where the focus goes.

**What would resolve it.** A concrete rule, modelled on the existing `held` state of a table ("shows
its lines while the caret is in a line typed in the editor"):
1. While the bar is open, an element shows its source exactly while it holds the current match. It
   returns as soon as the current match leaves it (by F3, typing, or an edit), never because of
   Highlight all.
2. On Escape with the current match in the element, the selection is set in the source and the
   element is held in source until the caret and anchor leave it, as `held` does today. After that
   the existing rules apply.
3. Closing without a match, or with the match elsewhere, returns the element at once.
4. With the focus in a cell editor of a table that switches to source, the focus goes back to the
   find bar if it came from there, otherwise to the editor at the cell's position.

Amend the S009 front-matter and table-cell scenarios accordingly and test each transition.

## O9 - failure - minor

**Objection.** Highlights are "painted (decorations pass or extra format ranges that never touch the
document); they never change geometry" (story:269–270). The story gives a text colour for the
current match (`current-text`) and for other matches (`match-text`, story:251–257). A match's source
range contains concealed characters. Story:176 says so for `rot** und`, and the story example
`wichtig: denken` covers `wichtig:** denken`. A foreground set over those characters undoes the
concealment.

**Evidence.** Concealment is a transparent foreground, plus a 1 px font for `Collapse`
(`src/ui/editor_formats.cpp:90–94` and `:335–338`; architecture.md, "Concealing markers"). A
`QTextLayout::FormatRange` or an extra selection with a foreground, merged over those ranges, paints
the markers again: collapsed `**` as 1 px coloured specks, and a kept-space marker in full. S009,
"no visible residue", forbids that. The story's S009 scenario (story:243–247) checks only that lines
do not move or change height.

**What would resolve it.** Require that the highlight's foreground applies only to the visible
segments (source range minus the line's hidden ranges). The background may cover the whole range,
or also only the visible segments for a clean look. Extend the story's S009 scenario: with matches
covering concealed delimiters, the rendered line shows no marker pixels (the existing residue check
of S009, run with highlights on).

## O10 - failure - minor

**Objection.** The story lists the shortcut questions as open (story:280–281). Taken literally,
story:32, story:37 and story:169 do not work on macOS. Typing would break, and stepping from the
editor would be unavailable on real hardware, while offscreen tests still pass.

**Evidence.** In Qt on macOS, `Qt::AltModifier` is Option. Option+C and Option+A type `ç` and `å` on
the US and German layouts. A `QShortcut` or `QAction` on Alt+C or Alt+A consumes that key event
before the search field gets the text, so these characters cannot be typed into the field. The
top-row F3 is a system key on Mac keyboards by default (Mission Control). The platform's find-next
binding is `QKeySequence::FindNext`, Cmd+G (`FindPrevious`: Shift+Cmd+G), as story:280–281 notes.
Story:137 requires that F3 "also work[s] while the focus is in the editor". On a Mac that is
reachable only with fn. The CI tests synthesise keys (`tests/ui/key_input.hpp`) and would not notice
any of this.

**What would resolve it.** Bind next and previous to `QKeySequence::FindNext` and
`QKeySequence::FindPrevious` on all platforms, plus F3 and Shift+F3 where they are not already part
of the platform binding, and Enter and Shift+Enter in the field. On Windows and Linux, give the
toggles mnemonics ("Match &case", "&Highlight all"), which yields Alt+C and Alt+A without stealing
text. On macOS, use Cmd+Option+C and Cmd+Option+A, or no shortcut. Write the scenarios in terms of
the platform bindings.

## O11 - failure - minor

**Objection.** Decision 9 keeps the caret and the selection still while stepping (story:41–43,
story:127). The editor's accessibility comes from Qt's `QTextEdit` (ADR 0003, decision 1: caret,
selection, … "accessibility stay Qt's"), which reports the caret and the selection. With decision 9,
nothing tells a screen reader where the current match is or what it says. The accessibility
scenario (story:238–241) covers only names, toggle states and the count.

**Evidence.** ADR 0003, decision 1. S009 has a screen-reader gate on all three platforms (S009,
"screen reader"). In Firefox, the model the story cites, the found text becomes the page's selection,
and assistive technology reads that. Here an "n of m" announcement is all a screen-reader user gets.

**What would resolve it.** Announce the context of the current match with the count, for example
"3 of 12: … line text around the match …", through an accessibility announcement
(`QAccessibleAnnouncementEvent`; check that its introducing Qt version is within the Qt ≥ 6.8
minimum) or the label's name change. Add this to the accessibility scenario. Alternatively, record
in the story that decision 9 accepts this limitation for screen-reader users until a later story.

## O12 - operational - minor

**Objection.** The token proposal (story:249–257) does not meet the theming baseline as written.
The fallbacks do not line up with the tokens, some values are missing or are not values, and the
fallback for the highlight text does not ensure the required contrast.

**Evidence.**
- There are 14 tokens before the highlight colours (`background` … `toggle-active-background`) but
  13 fallbacks. Paired in order, `padding` gets `layout.spacing`, and `spacing` gets
  `colors.text`, a colour for a length. One fallback is missing, probably for `spacing` or
  `padding`.
- `match-text` has no fallback. "a pale red" is not a colour value. "dark values of their own" names
  no values.
- Visible states have no tokens: hover and pressed backgrounds of the buttons, which the toolbar has
  (`toolbar.hover-background`, `toolbar.active-background`, theming.md); the focus border of the
  field; the field font and size; the size of icons and buttons. The brief requires a token for
  "every property it visibly has" (section 8).
- `current-text` falls back to `colors.text`, and fallbacks resolve at use (ADR 0004, decision 4). A
  user theme on `base = "light"` that sets light text gets light text on `#FFD43B`, below the 4.5:1
  that story:235–236 requires. The scenario cannot hold for every theme with these fallbacks.

**What would resolve it.** Write the table in the form of theming.md (token, type, range, fallback),
one row per token. Add the hover, pressed and focus states and the font and size tokens. Give
`current-text` and `match-text` a fixed dark fallback (for example `#1a1a1a`) instead of
`colors.text`, and set explicit values in the built-in dark theme. Limit the contrast check to the
built-in themes, or have the theme validator warn when a theme's highlight pair falls below 4.5:1.

## Capped concerns

- `LinkTarget::destination` does not resolve entities, although story:70 says it does: the probe
  keeps `hand&amp;buch`, while CommonMark resolves it. The rule and its test must state which text a
  destination unit has (and S022 may need a fix).
- Case folding (ADR 0008): if folding comes from `QChar::toCaseFolded` passed in from the UI, the
  core matcher's case rules cannot be tested on the Qt-free `core-sanitize` preset, and they follow
  the Unicode version of Qt 6.8 versus 6.12. A table generated from a pinned `CaseFolding.txt`
  (C+S, Unicode licence, GPL-compatible) in the core avoids both problems.
- Keyboard routing outside the bar: Escape with the focus in the editor (Firefox closes the bar) and
  Escape in a cell editor, which today emits `leaveBelow` (`src/ui/table_view.cpp:524–525`), are not
  specified. Neither is "the caret" for story:105 when the focus is in a cell editor.
- The budgets state only the hard 50 ms maximum (story:49, story:226). The established pattern for
  new interactions is "≤ 16 ms p95, 50 ms max (hard)" (performance.md, `s023_*`/`s024_*`). "Time to
  the first match" has no target.
- Short search texts (`e`) in 10 MB produce matches in the order of 10^5–10^6. Firefox caps the
  count. The story sets no cap and no bound on memory or on highlight-all painting.
- `parseProperties` is not sliced; from about 25,000 keys one pass exceeds 50 ms (performance.md,
  S024). If the search re-reads the front matter on each key in the field, the key budget breaks.
  It should reuse the embedder's parsed `Properties`.
- Matches that span an embedded element's edge, from a paragraph into a table's header cell or from
  the last property value across `---` into the body via decision 4, leave anchor and caret in
  different elements on Escape. `PropertiesEmbedder` shows the YAML only if both are inside.
- Whitespace extent is not defined. For a leading or trailing whitespace run in the search text, the
  story does not say whether the match (highlight, Escape selection) includes the whole whitespace
  run of the text, one character, or nothing at a line boundary.

## Explicitly not objecting to

- Decision 1 (searching the concealed view, also on the caret line), and defining it through
  `LineRender` hidden ranges. For non-table, non-front-matter lines the probe confirms that the
  concealed text is what the rules list: `# Titel` → `Titel`, `\*` → `*`, `~~weg~~` → `weg`, fences
  → empty, setext underline and thematic break → empty, `===` without a paragraph kept, `1. item`,
  `> quote` and `&auml;` kept. Future elements will change the search automatically.
- The search does not depend on setext levels found from below: `setextLevel` affects only heading
  runs (`element_registry.cpp:443–446`), not hidden ranges, so the search needs no unbounded
  lookahead.
- Decision 6's simple case folding (C+S, one code point to one). It keeps the folded and source
  texts aligned by code point, which makes mapping matches back trivial. The stated examples
  (`Σ`/`σ`/`ς`, `ẞ`/`ß`, `ä` ≠ `a`) are consistent with it.
- Keeping the search on the UI thread in slices instead of a core-owned snapshot. ADR 0002 names
  background search as a reason to reconsider, but the measured core cost (about 184 ms to render
  10 MB, 32 ms for a state walk) fits the slicing pattern used elsewhere.
- Defining whitespace by the Unicode `White_Space` property and excluding U+200B and U+FEFF, and
  making a whitespace-only search text find nothing.
- The "search never changes the document" scenario (ADR 0002), with undo stack and modified flag
  included. Showing a table or properties block as source writes nothing (ADR 0005, decision 2;
  `PropertiesEmbedder` writes nothing).

## Disposition of the capped concerns

- Entities in link destinations: documented. The story no longer claims they are resolved;
  destinations are searched with escapes resolved and entities as written, as hitokoto shows
  entities everywhere (S021, S022). Decoding entities is not part of S028.
- Case folding: fixed by ADR 0008 (Accepted with S028): a table generated from Unicode's `CaseFolding.txt`
  (C and S) in the core, pinned to Unicode 18.0.0, with a UI test that lists differences to
  `QChar::toCaseFolded`.
- Keys outside the bar: fixed in the story. `Escape` with the focus in the editor or a cell keeps its
  meaning and does not close the bar; with the focus in a cell editor the search starts at the start
  of that cell.
- Budgets: fixed. Keys in the search field: p95 within one frame (16 ms), maximum 50 ms (hard); time
  to the first match measured, time to the full count informational.
- Very many matches: fixed by the requirement "Edits and caching" (counts per block, positions only
  for visible blocks and the current match) and the scenario with a one-letter search text.
- `parseProperties` per key: fixed. The search uses the `Properties` the embedder holds or parses
  once per change of the front matter.
- Matches across an embedded element's edge: fixed by the hard edges (Armin, see O5).
- Extent of leading and trailing whitespace: fixed. It is a condition, not part of the match; the
  highlight and the selection start and end at non-whitespace characters.
