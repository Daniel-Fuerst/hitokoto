---
story: S028
title: "Objection record: S028 find in the open file"
mode: code
spec: docs/stories/028-find-in-file.md
branch: agents/s028-find-in-file
commits_examined: "S028: red tests for find in the open file; S028: find in the open file"
date: 2026-11-07
reviewer: Advocatus Diaboli (independent subagent)
dispositioned: 2026-11-07 (Claude, after Armin's go-ahead to implement; red tests first)
objections:
  - id: O1
    category: design
    severity: minor
    title: "A match that crosses a line end swallows the link destinations of that line"
    disposition: fixed
    disposition_rationale: "Core: scanUnits scans the destination units a match went past on its own, so their matches are kept (find.cpp, Matcher::run). Core test ADestinationSkippedByAMatchAcrossALineEndIsStillSearched."
  - id: O2
    category: design
    severity: minor
    title: "The screen-reader context reads skipped link destinations into the text around the match"
    disposition: fixed
    disposition_rationale: "Core: matchContext leaves out destination units after the first unit of the match. Core test MatchContextLeavesOutSkippedDestinations."
  - id: O3
    category: design
    severity: minor
    title: "Highlight all paints the whole property row, not the value or the chip with the match"
    disposition: fixed
    disposition_rationale: "PropertiesView::setFindMarks takes the row and the part (key, value, chip index); the controller maps each property unit to its row and part once per front-matter change (FrontMatter::parts). A marked chip gets find.match-background as its background. Story scenario and theming.md now name the key too. UI test highlightAllMarksTheKeyTheValueOrTheChip (PropertiesView::findMarkRects)."
  - id: O4
    category: failure
    severity: major
    title: "After an edit, a multi-word match that starts above a line with empty concealed text is lost"
    disposition: fixed
    disposition_rationale: "The walk up from an edit counts a line as a word only if its concealed text holds a non-whitespace character outside link destinations (a table row: its first cell), at most 2,000 lines, then from the top. UI test anEditCompletesAMatchAcrossLinesWithoutConcealedText (thematic break, fence, blank lines)."
  - id: O5
    category: failure
    severity: major
    title: "An edit that turns the line above into a table header is never searched as a table"
    disposition: fixed
    disposition_rationale: "An edit restarts at least one line above the edited line, which is rendered again with the edited line as its next line; the current match is kept only above the restart line. UI test aDelimiterRowTypedOrDeletedMakesOrBreaksATable; the random edit test makes and breaks tables."
  - id: O6
    category: failure
    severity: major
    title: "Front matter closed or made valid by an edit below line 1 is still searched as text"
    disposition: fixed
    disposition_rationale: "Front matter is kept per change (FrontMatter): line 0 opens it or not, its closing line, the YAML valid or not. An edit of line 0 (when it opens front matter before or after), of the block or of the line after it, or, while it is not closed, of any line starting with ---, reads it again and restarts the search. UI test frontMatterClosedOrFixedByAnEditIsSearchedAsProperties (p3, p4); the random edit test starts with front matter in every third seed."
  - id: O7
    category: failure
    severity: major
    title: "The pass ahead can pick a current match the reference does not have; the count shows 0 of 1 and stepping is dead"
    disposition: fixed
    disposition_rationale: "The pass ahead starts above the caret by the same word walk (with the styler's states), and a row of a table with the table's column count. If the pass from the start does not confirm its match on that line (start, end and inDestination), the match is dropped and found again from the anchor. UI test thePassAheadOnlyPicksMatchesOfTheDocument (p12)."
  - id: O8
    category: failure
    severity: major
    title: "In a long table every match lookup re-reads the table from its header; highlight all becomes quadratic and breaks the 50 ms budget"
    disposition: fixed
    disposition_rationale: "Core: FindUnitBuilder::continueTable starts at a row with the header's column count; lookups and restarts in a table start at the row (or above it while a match reaches in), not at the header. Highlight all marks cells only for the rows on screen. Core test ABuilderContinuedInATableBuildsItsRowsAsFromTheHeader; UI test aLongTableLooksUpMatchesFromTheirRow (lines rendered per step, FindController::lookupRenders); benchmarks s028_table_field_typing_max_ms and s028_table_step_max_ms on the 1,000-row table with highlight all, hard 50 ms."
  - id: O9
    category: failure
    severity: major
    title: "Next match before the count is complete works up to 60 ms on the key; the benchmark only measures it after completion"
    disposition: fixed
    disposition_rationale: "A step works the pass for at most 8 ms in all (the key budget); what it cannot take yet is counted in pendingSteps_ and taken in order once the pass gets there, also before the current match is known. UI test stepsBeforeTheCountIsCompleteAreQueued; benchmark s028_step_incomplete_max_ms (Enter right after a new search text), hard 50 ms."
  - id: O10
    category: failure
    severity: minor
    title: "After Escape into a table the source is released when the caret leaves the match lines, not the table"
    disposition: fixed
    disposition_rationale: "TableEmbedder::setFindHold widens a hold after Escape to the whole table. UI test escapeIntoATableHoldsTheWholeTable (p13)."
  - id: O11
    category: operational
    severity: minor
    title: "The edit and S009 tests cannot catch O4 to O6 and O10"
    disposition: fixed
    disposition_rationale: "editsUpdateTheMatchesLikeAFreshSearch: six queries (multi-word, leading whitespace), pieces that make and break tables and front matter, chained edits, highlight all in every second seed; every settled state is compared with findInDocument: count, current match, its number in the count text and the matches on screen (differenceToFresh). S009: the walk checks line geometry after every F3 and every hidden range (collapsed or keeping its space) under a highlight's text colour. findNextFromACellIntoItsOwnTable covers rule 4 of review O8."
  - id: O12
    category: cost
    severity: minor
    title: "Front matter is copied and parsed again on every search start and every match lookup, in one unbounded step"
    disposition: fixed
    disposition_rationale: "The front matter is walked, copied and parsed once per change of it (as PropertiesEmbedder::read does, in one step), not per key; its units and matches are kept for lookups (frontMatches). UI test frontMatterIsParsedOncePerChange (FindController::propertyParses)."
---

# Review S028 — Find in the open file (code)

Scope: the commits "S028: red tests for find in the open file" (red tests with stubs) and "S028:
find in the open file" (implementation) on `agents/s028-find-in-file`, read against the confirmed story
`docs/stories/028-find-in-file.md`, its spec review record and dispositions, ADR 0008, CLAUDE.md,
`docs/architecture.md`, ADRs 0002–0006, S009, `docs/theming.md` and `docs/performance.md`.

Probes were run outside the repo, in a scratch folder:

- `probe/tst_probe.cpp`: a Qt Test linked read-only against `build/ci` (Release, Qt 6.12, offscreen).
  It compares `FindController` with `core::findInDocument` after edits and times keys.
- `core/dump.cpp`: compares the S024 parse before (`properties.cpp` from the spec review commit)
  and after, on generated YAML, and checks every `PropertySourceRun` against the source (ASan and
  UBSan).
- `core/dest.cpp`: probes the core matcher directly.

Line numbers refer to the implementation commit.

## O1 - design - minor

**Objection.** The story makes a link destination a side unit with hard edges, "the text around its
link goes on past it as if it were not there". The matcher does skip destinations inside a
whitespace run. When a match ends below, though, the scan resumes behind that match and never visits
the destination units it skipped. A match that lies entirely inside such a destination is dropped,
depending on what the next line begins with.

**Evidence.**

- `src/core/src/find.cpp:470-476`: `whitespace()` skips Destination units.
- `find.cpp:410-413`: after a match, `c = *end`, so the scan continues in the later unit.
- Core probe `dest.cpp`, query `a a`:
  - Lines `x [y](<a a>) a` / `b` give one match, `(0,3)-(0,4)@dest`.
  - Lines `x [y](<a a>) a` / `a` give one match, `(0,13)-(1,1)`. The destination match is gone.
- The UI agrees with the core here (probe `p9`, controller 1, reference 1). The behaviour is
  consistent but not what the unit model promises. The two matches do not overlap in the searched
  text: the destination is a separate unit.
- The trigger is rare (it needs whitespace inside a `<…>` destination). The rule is still undefined,
  and the core tests (`MatchesDoNotOverlap`, `NoMatchCrossesTheEdgesOfADestination`) do not pin it.

**What would resolve it.** Either decide in the story that a match crossing a line end consumes
the destinations of that line, and add a core test for it, or scan the skipped destination units
separately so that their matches are kept.

## O2 - design - minor

**Objection.** The announcement for screen readers (review O11) is to read "the concealed text
around the match". For a match that runs past a link, `matchContext` joins every unit from the
match's first to its last unit, including the destination units that the match skipped. The URL is
read out in the middle of the sentence.

**Evidence.**

- `find.cpp:586-602`: the loop runs over `match.unit..match.endUnit` and does not check the unit kind.
- Probe `p10`: document `Absatz [Doku](https://example.com/handbuch) hier` / `weiter geht es`, search
  `hier weiter`. Announcement: `1 of 1: Absatz Doku hier https://example.com/handbuch weiter geht es`.
- `MatchContextForScreenReaders` (core) covers only single-line matches without links.

**What would resolve it.** Skip Destination units in `matchContext` unless the match lies in one,
and add a core test with a link before a line end.

## O3 - design - minor

**Objection.** The highlight-all scenario says that "a cell, a property value or a chip that
contains a match … gets `find.match-background` as its background". For properties, the
implementation fills the whole row rectangle, key and value. It does this for a match in the key, in
a value, or in one chip of a list alike. The user cannot see which chip, or whether the key or the
value, matched.

**Evidence.**

- `src/ui/find_controller.cpp:1019-1029`: the row is the last property whose `keyLine` is at or
  before the match's line. No column, item or key/value distinction is kept.
- `src/ui/properties_view.cpp:451`: `painter.fillRect(it->rect, findBackground_)` fills the row's
  whole rect.
- `docs/theming.md` (S028 paragraph) repeats the story's "a property value or a chip".
- No UI test covers highlight all in properties. `highlightAllMarksCellsInTheTableView` covers only
  tables.

**What would resolve it.** Mark the key, the value or the chip that holds the match: carry the
unit index from `propertySourceUnits` and map it to the parts of the row. Otherwise amend the story
and `theming.md` to "a row", and add a UI test.

## O4 - failure - major

**Objection.** The story requires that after an edit "the matches and the count follow the edit …
equal to a fresh search of the edited document once settled". To restart above an edit,
`onContentsChange` walks up one non-blank raw line per extra word of the search text. Lines whose
concealed text is empty also act as pure whitespace in the matcher, but the walk counts them as
words: thematic breaks, setext underlines, fence lines and lines of hidden syntax only. The restart
lands below the line where the match starts. The pass then settles on the old (wrong) results.

**Evidence.**

- `find_controller.cpp:784-798`: `words` is decremented for every line whose raw `text().trimmed()`
  is non-empty. `!info.crossed` lets the walk stop early when no old match crossed into the line,
  which is always the case when the edit creates the match.
- Probe `p1`: document `foo` / blank / `***` / blank / `baz`, search `foo bar`, then edit `baz` to
  `bar` in the editor. Controller `No matches`, `findInDocument` 1.
- Probe `p1b`: `foo` / opening fence / `baz` / closing fence, same edit. Controller 0, reference 1.
- Matching across such lines is in the spec: whitespace runs match "across line breaks, blank lines
  and block boundaries" (decision 4). The core test `WhitespaceRunsMatchAcrossLinesBlankLinesAndBlocks`
  checks it for a fresh search.

**What would resolve it.** Count a line as a word only if its concealed text holds a
non-whitespace character. Alternatively, restart at the start of the run of lines that could hold
the first word, using the cached per-line information, or simply at the last line above the edit
that has a match start or a non-empty unit outside the walk. Add a UI test with a multi-word query
and a break or fence between the words.

## O5 - failure - major

**Objection.** A line's units depend on the line below it: a table header is recognised only with a
delimiter row underneath (`LineContext::nextLine`). After an edit, the pass restarts at the edited
line with that line's cached entering state, which was computed before the edit. The line above is
never rendered again. Typing a delimiter row under a pipe line leaves both lines searched as
paragraph text, against the story's own edit scenario ("a delimiter row that makes a table").

**Evidence.**

- `find_controller.cpp:745, 753-755`: the edited first line keeps `firstInfo.entering`, the state
  from before the edit.
- `find_controller.cpp:778-798`: for a one-word query, `restart = first`.
- `restartFrom` walks up only while the cached entering state is inside a table (`:361-365`). That
  cached state is the old, non-table one.
- Probe `p2`: document `| a | b |` / empty / `x`, search `|` (3 matches, correct). Type `|-|-|` on
  line 1. Controller `1 of 6`, `findInDocument` 0. Pipes and the delimiter row are counted, the
  opposite of "pipes and the delimiter row are never part of a match".
- The same holds in reverse when a delimiter row is deleted. Setext underlines are the other
  `nextLine` dependency.

**What would resolve it.** Restart at least one line above the edit, and re-derive the entering
state of the edited line from the line above (or from the styler's fresh state) rather than from
the cache. Treat a change of a line's `table`/`tableHeader` result as an explicit trigger, as the
story's requirement "a table's extent changing are explicit triggers" asks. Add the delimiter-row
case to the UI edit test.

## O6 - failure - major

**Objection.** The requirement names "front matter confirmed or ended" as an explicit trigger. The
only front-matter handling on edits is a full restart when the restart line is at or above
`frontMatterClose_ + 1`. When front matter is not being searched as properties
(`frontMatterClose_ == -1`), edits further down never re-check it: the block is unclosed, its YAML is
invalid, or the option is off. Two common edits leave the search stale: typing the closing `---`
later, or fixing the YAML of closed front matter.

**Evidence.**

- `find_controller.cpp:799-801`: `restart == 0 || (frontMatterClose_ >= 0 && restart <= frontMatterClose_ + 1)`.
- `frontMatterStep` (`:381-445`) runs only from `restartFrom(0)`.
- Probe `p3`: `---` / `title: x` / empty / `body`, search `title:` (1, correct while unclosed). Type
  `---` on line 2. Controller `1 of 1`, reference 0.
- Probe `p4`: `---` / `title: [x` / `---` / `body` (invalid YAML, text mode), search `title:`. Type `]`
  to make it valid. Controller `1 of 1`, reference 0.
- The story's scenario names only "a change to the first line that ends front matter". The UI test
  does not edit front matter at all (see O11).

**What would resolve it.** Fall back to `startSearch` for any edit while the document starts with
an open front-matter state, up to and including the first line that could close it. Or ask the
styler or `PropertiesEmbedder` whether the confirmed close or its error changed, and restart from 0
then. Add `p3` and `p4` as UI tests.

## O7 - failure - major

**Objection.** The pass ahead (review O6) starts at the caret's line with a fresh unit builder and
offset 0. It takes the first match at or after the anchor. The reference scans from the document
start, and its non-overlap rule lets a match that starts above the caret consume text on the caret's
line. The pass ahead can therefore pick a match the reference does not contain. When the pass from
the start reaches that line, `finalizeUpTo` never finds it among the block's matches, so
`currentOrdinal_` stays -1 for good:

- The count reads `0 of 1`.
- `step` returns with `pendingStep_` set and never moves: Enter, F3 and the buttons do nothing.
- The highlight marks text that is not a match.

**Evidence.**

- `find_controller.cpp:283-300`: only for an anchor at line 2000 or later with a complete styler.
- `:335-349`: the current match is set from `aheadFound_` with ordinal -1.
- `:540-548`: the ordinal is looked up by equal start and end.
- `:163-168`: no step while `currentOrdinal_ < 0`.
- Probe `p12`: 3,000 lines, then `a` / `a a` / `ende`, caret at the start of `a a`, search `a a`.
  Reference: one match `(3000,0)-(3001,1)`. Controller: current `(3001,0)-(3001,3)`, count `0 of 1`.
  After Enter, still `0 of 1`, same current.
- The same path runs on every key in the field (`startSearch(current_->start)`, `:62-65`). Typing
  over a line end with the caret below line 2000 is enough. No test exercises the pass ahead with a
  multi-word query.

**What would resolve it.** Start the pass ahead at a line where no match from above can reach in. At
minimum, walk up as `matchesOfBlock` does, and also over lines a multi-word query could span.
Alternatively, accept a match from the pass ahead only once it is confirmed by the pass from the
start; while unconfirmed, fall back to `resolveCurrentFromCounts` semantics when it is not found.
Add a UI test with the caret below line 2000 and a match crossing into its line.

## O8 - failure - major

**Objection.** Positions are kept "only for the visible blocks and the current match" and are
recomputed by `matchesOfBlock`. For any line inside a table, it walks back to the table's header and
renders every row from there, so one lookup costs O(rows above). `updateEmbeddedMarks` calls it for
every table line with a match, which makes it O(rows²). It runs on every refresh: every slice,
scroll, caret move and key. In a long table with highlight all on, keys and steps miss the hard
50 ms budget by a wide margin. Without highlight all, a step still grows linearly with the row.

**Evidence.**

- `find_controller.cpp:653-657`: the walk back over `isTableInside(entering)`.
- `:1041-1055`: a `matchesOfBlock(line)` call for each table line with a match.
- `:939-950`: a call for each visible line in `updateHighlights`.
- `:620`: `slice()` refreshes after each slice.
- Probe `p6`, a table of 4,000 rows each holding `x`, search `x`, Release build:

  | Action | Time |
  |---|---|
  | Step into the table | 3.3 ms |
  | Step at row ~3,000 | 16.4 ms |
  | Turning on highlight all | 127.5 ms |
  | One step with highlight all | 205.9 ms |
  | Retyping `x` with highlight all | 280.7 ms |

- The `s028_*` benchmarks use only `largeDocument`, which has no tables, although
  `tableDocument(1000)` exists in the same file (`bench/hitokoto_bench.cpp:85, 846`). The budget gap
  is invisible to the gate.

**What would resolve it.** Cache the units or match positions per table, or at least the per-row
matches of the table that is on screen. Alternatively, start the walk at the row itself: cells
within a row are soft-bounded, rows are separate lines, and only the column count from the header
is needed. Mark cells from the matches the pass already found. Add an `s028_*` benchmark on a
1,000-row or larger table with highlight all.

## O9 - failure - major

**Objection.** The story's large-file scenario says no key press may take longer than 50 ms when you
"step with `F3`" in a 10 MB document. Before the count is complete, `step` runs `work(30 ms)` when
the current match has no ordinal yet. It runs `work(30 ms)` again when the next match is not on the
same line. Both happen inside the key event, before the refresh. The benchmark `s028_step_max_ms`
steps only after `measureFindSearch` has waited for the full count, so it never sees this path.

**Evidence.**

- `find_controller.cpp:36`: `kStepNanoseconds = 30'000'000`.
- `:163-165` and `:177-179`: up to two such calls per step.
- `bench/hitokoto_bench.cpp:704` (wait for `isComplete`) runs before `:1029` (`measureFindSteps`).
- Probe `p7`, the benchmark's 10 MB document, caret in the middle: type `ipsum`, then press Enter 20
  times at once. Worst key 62.4 / 62.3 / 62.6 ms in three runs. `performance.md` reports 5.0–9.2 ms
  as the step maximum.
- Related: a step requested while incomplete is stored in a single `pendingStep_` (`:167, 184, 199`).
  Pressing F3 three times before the pass catches up takes one step.

**What would resolve it.** Bound the work on a step to the same per-key budget as typing (at most
8 ms in total). Leave the rest to slices with a queued step count rather than one pending direction.
Add a benchmark sample of steps taken right after a new search text.

## O10 - failure - minor

**Objection.** Review O8, as decided: after `Escape` into a table, "the element keeps its source
until the caret and the anchor have both left it". The implementation holds the lines of the match,
not the table. Moving the caret to another row of the same table releases the hold. The table then
switches back to cells at once and the focus jumps into a cell editor. The story's implementation
note ("until the caret and the anchor leave it") describes the intended behaviour, not this one.

**Evidence.**

- `find_controller.cpp:137`: `setFindHold(match->start.line, match->end.line, true)`.
- `src/ui/table_embedder.cpp:681-693`: `outside(n)` compares with `findFirst_`/`findLast_`, the
  match lines.
- Probe `p13`: table with rows `rot` and `gelb`, search `rot`, Escape (selection `rot`, source shown).
  One Down key gives caret on line 5, source shown 0, focus `hitokoto::ui::CellEditor`.
- `escapeIntoATableKeepsTheSourceUntilTheCaretLeaves` moves the caret out of the table entirely, so
  it passes.

**What would resolve it.** Pass the table's header and last line, as `table.found` already
computes, rather than the match lines, to the hold after Escape. Test a caret move within the table.

## O11 - operational - minor

**Objection.** The UI test that is meant to guarantee "equal to a fresh search … once settled"
cannot fail for the classes of edits the story names, and the S009 walk checks less than its
scenario. O4–O6 and O10 pass the suite.

**Evidence.**

- `tests/ui/tst_s028_find_in_file.cpp:628-657`:
  - It uses a single-word query (`wort`), so the word walk of O4 is never exercised.
  - Its only delimiter piece `| - |` has one column under a two-column header, so it never makes a
    table (O5).
  - No piece edits or closes front matter (O6).
  - It compares only `matchCount()` and that the current match exists, not positions or the index.
- `tests/ui/tst_s009_concealment.cpp:266-312` (`findHighlightsWhileWalking`): it checks line
  geometry once, after highlight all is turned on, not after the F3 steps in the walk. The S009
  scenario asks that "when the current match moves … no line moves". Its marker check looks for
  collapsed (1 px) formats only, not kept-space markers, and checks formats, not "marker pixels".
- No test covers rule 4 of review O8: F3 from a cell editor into a match in the same table, which
  `TableEmbedder::release` handles by moving the caret into the line.
- The core test `RandomDocumentsFindWhatTheyConceal` derives its expectation from `documentUnits`
  itself. Units are compared with `renderDocument` only on one fixed document
  (`DocumentUnitsFollowRenderDocument`), with no random tables or front matter.

**What would resolve it.** Extend the random edit test:

- multi-word queries;
- pieces that make and break tables (with matching column counts), and front-matter delimiters on
  line 0 and later lines;
- a comparison of every match (start, end) and of `countText()` with `findInDocument`.

Check geometry inside the S009 walk after each step. Add the cell-editor case.

## O12 - cost - minor

**Objection.** The requirement says "Properties are not parsed again per key: the search uses the
`Properties` the embedder holds … or parses once per change of the front matter". Instead, every
`startSearch` (each key in the field) walks to the closing delimiter again. When it closes, one step
with no time check does all of the following:

- copies every YAML line;
- runs `parseProperties`;
- recomputes the entering states of all those lines;
- parses again through `addProperties` → `propertySourceUnits`.

`matchesOfBlock` parses a third time on every call for a line of the front matter (highlight,
announcement, step). The step's cost grows with the distance to the closing `---`. In a document
whose first line is a thematic break `---` and whose next `---` is far below, the copy alone stalls
the UI.

**Evidence.**

- `find_controller.cpp:396-427` (no timer check inside), `:254` (reset per search), `:649-651`.
- Probe `p8b`: front matter of 20,000 `key: value` lines, find closed: worst event-loop stall 13.0 ms.
  After one key in the field: one stall of 120.7 ms.
- Probe `p8c`: the 10 MB benchmark document between a `---` on line 0 and a `---` at the end (the
  reference treats it as front matter, `renders[0].frontMatter=1`). Worst stall 237.2 ms per search
  start, though each key itself stays at 9.4 ms.

**What would resolve it.** Take the closing line and `Properties` (or the parsed units) from
`PropertiesEmbedder` / the styler, as the requirement says. Cache `propertyUnits` per front-matter
revision for `matchesOfBlock`. Slice the copy and parse, or give up on a "front matter" longer than
a bound and treat it as text, as S024 might.

## Capped concerns

- Prefix sums for the index `n` (requirement "Edits and caching") are not implemented. `matchCount()`
  and `currentIndex()` accumulate over all blocks, and `updateBar` calls `matchCount()` twice per
  refresh (`find_controller.cpp:825-839, 924-927`).
- `cellText` duplicates the substitutions of `tableCellToText` (`find.cpp:162-194` against
  `elements/table.cpp:240-269`): two copies to keep in sync.
- The find hold in `TableEmbedder`/`PropertiesEmbedder` is held as line numbers. An edit above it
  resets `current_` without calling `setFindHold` (`find_controller.cpp:771`), so until the match is
  found again the hold can point at shifted lines or another table. Not probed.
- The bar's buttons and toggles are `Qt::NoFocus` (`find_bar.cpp:31`). Keyboard users cannot reach
  them with Tab, only through the shortcuts.
- No announcement is made for "No matches", and none until the count is complete
  (`find_controller.cpp:918`). In a 10 MB file, the announcement of a step waits about 1.2 s.
- An edit below the part the first pass has reached (`!info.known`) restarts the whole search from
  line 0 (`find_controller.cpp:787-789, 799-800`). Typing while the count is incomplete keeps
  restarting it.
- Matches are identified by start and end only. `[handbuch](handbuch)` yields a text match and a
  destination match with the same range: the ordinal lookup picks the last (`:543-546`), and a step
  between them is not announced.
- `startAhead` tests `frontMatterClose_ >= 0` right after `startSearch` reset it to -1 (`:254, 284`).
  The condition is dead, and the pass ahead ignores front matter.

## Explicitly not objecting to

- **Skipping lines without a character of the search text** (`mayMatch`, `find_controller.cpp:491`,
  `:320`). It is sound. It applies only with no pending units, never to table lines or the line
  after a table. Every unit character comes from the line's own raw text (escapes resolved to
  characters that are present, entities as written). Surrogates always count as a possible match.
- **The YAML source maps and S024.** `parseProperties` output is byte-identical before and after on
  3,200 generated documents (escapes, `''`, folded and literal blocks with chomping, quoted keys,
  multi-line flow collections, comments, trailing spaces in quoted scalars). Every one-to-one
  `PropertySourceRun` equals its source text, and runs cover each unit without gaps (ASan/UBSan
  clean). The only irregularity is that a block scalar's chomped final newline is an atomic run with
  an empty source. It is harmless, because a match never starts or ends on whitespace.
- **ADR 0002.** Highlights are extra selections and embedder holds. The document, undo stack and
  modified flag stay untouched, and `theSearchNeverChangesTheDocument` checks bytes after saving.
- **The core matcher's basic rules:** words between whitespace runs, leading and trailing whitespace
  as conditions at unit edges, hard edges around tables and properties, surrogate pairs, and simple
  case folding from the generated Unicode 18.0.0 table compared with `QChar::toCaseFolded`.
- **Platform key bindings.** Cmd+Option+C/A on macOS, Alt+C/A elsewhere with
  `WidgetWithChildrenShortcut`, F3 added only where `FindNext`/`FindPrevious` lack it, Enter and
  Shift+Enter (keypad too) in the field.
- **Theming.** All 25 `[find]` tokens are parsed, documented with fallbacks, and resolved in
  `findStyle` with fixed highlight text colours. The built-in contrast is tested.
- **Highlight foreground only on visible characters** (review O9): `updateHighlights` splits the text
  colour around the styler's hidden ranges.

## Disposition of the capped concerns

- **Prefix sums for the index:** documented. `matchCount()` and `currentIndex()` add up the counts
  per line on each refresh; in the 10 MB benchmark document (about 300,000 lines) that is well under
  a millisecond and stays within the key budgets. Prefix sums remain an option if a budget needs
  them.
- **`cellText` duplicates `tableCellToText`:** documented. Both are covered by the core tests of
  cells; a change to one without the other shows there.
- **The hold after an edit above it:** fixed. When an edit takes the current match away until it is
  found again, the hold moves with the shifted lines (`onContentsChange`).
- **Buttons not reachable with Tab:** fixed. The bar's buttons take `Qt::TabFocus`. UI test
  `theBarsButtonsTakeTheFocusWithTab`.
- **No announcement for "No matches":** fixed. Announced once the count is complete. UI test
  `noMatchesIsAnnounced`. The announcement of a step still waits for the complete count, so that it
  can say "n of m": documented.
- **An edit below the part the pass has reached restarts from line 0:** fixed. The pass goes on and
  reads the edited lines when it gets there; it may not settle before them. An edit above it
  restarts near the edit; if the earlier pass had not finished, the new one reads on instead of
  jumping to where the earlier one stopped (its state there is not kept).
- **Matches identified by start and end only:** fixed. `inDestination` is compared too, so the
  text and the destination of `[handbuch](handbuch)` are two matches, and a step between them is
  announced. UI test `aLinkWhoseTextIsItsDestinationGivesTwoMatchesAndAnnouncesBoth`.
- **The dead front-matter check in `startAhead`:** fixed. The pass ahead uses the styler's confirmed
  closing line and does not start in or right after the front matter.
