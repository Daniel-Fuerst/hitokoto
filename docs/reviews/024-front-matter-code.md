---
title: "Objection record: S024 front matter shown as properties (code)"
mode: code
spec: docs/stories/024-front-matter.md
branch: agents/s024-front-matter
commits_examined: "f71cd38..235eb3e (18 commits: 1d599d0 in progress, 2ba10fb/afc7ea2 review O4, d5b7712/03d669e and 897478d/d24c0a8 S019, 74e656b/60bddd8 S023, d895455/8ef87d7 parser, 98b7350 tokens, c781444 editor, 7f0ec8f fuzzer, 362fae3/00995fc caret after opening, 235eb3e benchmark and docs); context 2eb4ec1 (story, ADR 0007), 06852d0 (spec review), f71cd38 (red tests)"
dispositioned: 2026-10-31 (Armin decided O2, O4, O6 and keeping the S019 and S023 fixes on this branch (O9); the others as proposed by Claude). Commit hashes in this record are those of the review copy in the cloud workspace; in the repository the commits carry the same titles.
date: 2026-10-31
reviewer: Advocatus Diaboli (independent agent)
evidence_tools:
  - git log/show/diff of f71cd38..235eb3e
  - build/ci (Qt 6.12, Release, offscreen) as built from 235eb3e: S024_FrontMatter 18/18 and tst_s024_front_matter 15/15 passed
  - a core probe outside the repo (g++ against a copy of properties.cpp with a JSON hook) compared with js-yaml 4.3.2 (CORE_SCHEMA) and ruamel.yaml 0.19.1 on 219 hand-made cases, 16,000 random front matters (three seeds) and 33 number formats
  - a core timing probe (parseProperties on 5,000 to 200,000 keys, nested values, 10 MB of prose, deep nesting with many blank and comment lines)
  - a Qt probe outside the repo linked against build/ci's libhitokoto_ui.a and libhitokoto_core.a (test::Fixture of tests/ui): typing below the block, long text values, tooltips, leaving large blocks, opening, replacing the document while loading, selections
  - timings on the 2 vCPU VM with a clang-tidy run in the background (load about 2); they are indicative, the comparisons with a control on the same run are what matters
objections:
  - id: O1
    category: failure
    severity: major
    title: "Typing on the line directly below the closing line re-reads and rebuilds the block on every key; from about 1,000 keys every key shows the YAML and moves the body"
    disposition: fixed
    disposition_rationale: "The embedder marks the YAML as changed, and the styler confirms the front matter again, only for an edit at or above the closing line; joining the line below to the closing line reports a position on the closing line. Test tst_s024_front_matter::typingDirectlyBelowTheBlockKeepsIt (x and Backspace on the line below 5,000 keys: the block stays shown and the body does not move after any key). s024_typing_* types on that line now. Commits \"S024 review O1 (code): ...\"."
  - id: O2
    category: failure
    severity: major
    title: "A long text value is laid out and painted whole: a block scalar of 2,000 lines costs about 45 ms on every repaint and key near the block, 147 ms on leaving it"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-31: show text values in full, cache their layout. A key or text value that fits on one line is drawn as it is; a longer one is laid out once per width into a QTextLayout whose lines are created across slices (at least 32 per slice), and a paint draws only the lines and chips in its area. Test tst_s024_front_matter::aLongTextValueIsLaidOutInSlicesAndPaintedOnScreenOnly (2,000 lines; PropertiesView::paintedTextLines()). New benchmark keys s024_long_text_typing_* and s024_leave_long_text_* with hard budgets of 50 ms; performance.md has the numbers. A new width or theme still lays out all rows at once (on a resize, not on a key)."
  - id: O3
    category: failure
    severity: minor
    title: "Reading the YAML is not sliced: the key that leaves the block, and a single idle slice, copy and parse the whole front matter"
    disposition: documented
    disposition_rationale: "The story's rule \"Long front matter\" now says that building takes at most 4 ms on a key and continues in idle slices, and that reading the YAML is done at once, on the key that leaves the block or in one idle slice. Measured on the development VM after editing a key: the leaving key stays under 50 ms up to 50,000 keys; the longest idle pass passes 50 ms from about 25,000 keys (about 500 KB of front matter). performance.md records the numbers, architecture.md and PropertiesEmbedder's header follow."
  - id: O4
    category: failure
    severity: minor
    title: "After opening a note whose front matter is directly followed by a table, the keyboard focus is in the table's first cell"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-31: when the line below the closing line belongs to a table, opening leaves the caret at the start of the document. PropertiesEmbedder::placeCaretBelow() asks the styler for that line. The case is in tst_s024_front_matter::openingPutsTheCaretBelowTheFrontMatter (caret at 0, no focus in the table); the story's rule \"After opening\" and its scenario say so."
  - id: O5
    category: failure
    severity: minor
    title: "The tooltip with the full JSON is taller than the screen for the project's own review records"
    disposition: fixed
    disposition_rationale: "The tooltip shows the JSON as JSON.stringify(value, null, 2) writes it, at most 20 lines, then a line \"…\" (core::prettyJson reads only that prefix); a click shows the YAML for the rest, the accessible name keeps the full JSON. Tests S024_FrontMatter.PrettyJsonForTheTooltip (against Node 22, and a value of 100,000 objects) and aReviewRecordShowsAsProperties (PropertiesView::toolTipText()); fuzz_render checks the cut. Story updated."
  - id: O6
    category: failure
    severity: minor
    title: "\"bad indentation\" is the badge for the most common mistakes, such as `title: Note: draft` or `\"C:\\path\"`; the story table was widened in the implementation commit"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-31: three new reasons, then a fourth for what remained: \"colon in a plain value (quote it)\", \"unknown escape\", \"text after a value\" and \"reserved character (quote it)\", all invalid. \"Bad indentation\" is left for indentation, including `: ` on a later line of a plain scalar (a key indented too far) and a block collection on its key's line (`a: - x`). Test S024_FrontMatter.InvalidYamlNamesTheMistake (each case with its line; js-yaml 4.3.2 and ruamel.yaml 0.19.1 reject all of them); two expectations of CasesFromTheDifferentialRun follow. The story's table lists the reasons."
  - id: O7
    category: failure
    severity: minor
    title: "A click on a block scalar or on a null with a comment does not put the caret where the story says"
    disposition: fixed
    disposition_rationale: "A top-level block scalar reports its first content line at the content's indentation, its header only when it has no content. For a null the code keeps the position directly after the colon; the story now says so (before a comment). Tests S024_FrontMatter.ClickPositionsOfBlockScalarsAndNulls and tst_s024_front_matter::aClickOnABlockScalarPutsTheCaretAtItsContent."
  - id: O8
    category: design
    severity: minor
    title: "The second embedded element is wired into EditorWidget and DocumentSession at about twenty places; the Claimant interface solves only hidden and reserved lines"
    disposition: documented
    disposition_rationale: "architecture.md, \"Adding a Markdown element\", step 4: an embedded element has an embedder class registered as BlockLayout::Claimant and is wired into EditorWidget, BlockStyler and (for properties) DocumentSession at the places listed there. The third embedded element (an image, a math block) introduces an embedder interface that EditorWidget iterates and moves tables and properties to it."
  - id: O9
    category: operational
    severity: minor
    title: "Fixes to S019 and S023 ride on the S024 branch without a note in their stories"
    disposition: fixed
    disposition_rationale: "Armin approved keeping the fixes on agents/s024-front-matter. S019 has a section \"Fixes found later\" and S023 a line under Implementation, each with its tests. The full suite ran on the branch's head (Qt 6.12 and 6.8.3)."
  - id: O10
    category: operational
    severity: minor
    title: "docs/architecture.md says each row of the block is a child widget; it is an accessible interface without a widget"
    disposition: fixed
    disposition_rationale: "architecture.md describes PropertiesView as one painting widget with text laid out once per width, and the rows as accessible interfaces registered on demand and deleted on reset and in the destructor. Since the clang-tidy run the view keeps the registered interfaces, not their ids, and the interfaces hold the view as a raw pointer (the view deletes them first); the styler removes the embedder's pending work in the embedder's destructor instead of guarding it with a QPointer."
  - id: O11
    category: operational
    severity: minor
    title: "The story's list of differences from js-yaml is incomplete: `-.5` and `+.5` are numbers, `1e400` is null"
    disposition: fixed
    disposition_rationale: "`-.5`, `+.5` and `1e400` are in the story's list of differences from js-yaml and in CasesFromTheDifferentialRun (numbers; JSON -0.5, 0.5 and null). The parser already followed the core schema and ruamel.yaml."
---

# Objection record: S024 front matter shown as properties (code)

Scope: the implementation of S024 on `agents/s024-front-matter`, commits f71cd38..235eb3e, against
`docs/stories/024-front-matter.md`, ADR 0007, the dispositions of the spec review
(`docs/reviews/024-front-matter-spec.md`, especially O1, O4, O6, O7, O8 and O12), ADR 0002, 0003,
0005 and 0006, the brief (sections 3, 4, 5 and 8) and stories 009, 018 and 023. I read the parser
(`properties.cpp`), the front matter element, the tokens, `PropertiesEmbedder`, `PropertiesView`,
the changes to `BlockLayout`, `TableEmbedder`, `BlockStyler`, `EditorWidget`, `DocumentSession` and
`MainWindow`, the fuzzer, the benchmark, the tests and the docs. I ran the S024 tests on build/ci and
checked behaviour with probes outside the repository: the parser against js-yaml and ruamel.yaml,
and the real editor driven through `test::Fixture`. Nothing in the repository was changed.

## O1 - failure - major

**Claim.** Every edit on the first line below the closing line marks the front matter as changed.
The next caret move then reveals the YAML, reads it again and rebuilds every row on the key. If the
rebuild takes more than 4 ms, the YAML stays visible until idle slices finish building. From about
1,000 keys that happens on every keystroke, so the whole block flips to YAML and back and the body
jumps on every key. This is exactly the line where "After opening" puts the caret.

**Evidence.**
- `properties_embedder.cpp:356-361`: `documentChanged` sets `dirty_` for an edit in any block
  `<= close_ + 1`. An edit on line `close_ + 1` cannot change the YAML: a Backspace that joins it
  to the closing line reports a position on the closing line, and the YAML is lines 1 to
  `close_ - 1`.
- `properties_embedder.cpp:276-283`: `caretMoved` sees `dirty_` and calls `work(kKeyNanoseconds)`.
  `:231-243`: `work` calls `reveal()` and `read()`, then `view_->build(timer, 4 ms)`. If the build
  does not finish, it returns `false` and the YAML stays shown ("the YAML shows until the block is
  built").
- The styler does the same independently: `block_styler.cpp:1034` re-confirms front matter for an
  edit at `first <= frontMatterClose_ + 1`, and `:980` sets `frontMatterClose_ = -1` until that
  confirmation finishes. With a confirmation longer than 4 ms, `confirmedClose()` returns -1
  (`properties_embedder.cpp:139-141`) and the next `caretMoved` reveals the block for that reason
  too.
- `properties_embedder.cpp:170-172`: after opening, the caret goes to line `close_ + 1`.
- The Qt probe (`probe typing N`) uses a note `---`, N keys, `---`, `# Heading`, `body`. It types
  `x` and Backspace in turn on the heading (`close + 1`) and checks the state right after each key,
  then on the line below as a control:

  ```text
  N =    20  close+1: median 1.9 ms   YAML shown after 0 of 20 keys    control 1.7 ms
  N =   200  close+1: median 8.1 ms   YAML shown after 0 of 20 keys    control 2.0 ms
  N = 1,000  close+1: median 8.7 ms   YAML shown after 20 of 20 keys, body moved after 20   control 2.5 ms
  N = 2,000  close+1: median 10.2 ms  YAML shown after 20 of 20 keys, body moved after 20   control 3.2 ms
  N = 5,000  close+1: median 15.3 ms, max 23.5 ms; YAML shown after 20 of 20 keys, body moved after 20   control 9.0 ms
  ```

- Story `:326-327`: the block is "rebuilt only when the caret leaves the block or the lines
  change". Story `:275-276` and the S009 scenario: "the lines below move only when the caret enters
  or leaves the block".
- Nothing catches this. The benchmark types two lines below the block, and its document has a blank
  line after `---` (`hitokoto_bench.cpp:169`, `:878` "the heading after the blank line"). The S009
  walk `frontMatterWhileWalking` presses navigation keys only and checks only settled states.

**Why it matters.** A note opens with the caret on its heading, and the user's first edit goes
there. With 200 properties every key on that line costs four times as much as elsewhere. With 1,000
or more, the block flickers through its full YAML on every key.

**Ask.**
- Mark the YAML dirty only for edits in blocks `<= close_`. In the styler, re-confirm only when the
  edit can change the closing line, or keep the last confirmation until a new one differs.
- Add a UI test that types on line `close + 1` of a block that cannot be built within 4 ms and
  asserts that the block stays shown after each key, before settling.
- Let `s024_typing_*` type on the line directly below the closing line.

## O2 - failure - major

**Claim.** Review O1 capped JSON at three lines and review O8 asked for building and layout within
the budget, but a text value has no cap. `layoutRow` measures the whole value with
`QFontMetricsF::boundingRect`. `paintRow` lays out the whole value again with `QPainter::drawText`
on every paint of the row. `build` checks the clock only every 16 rows. A long block scalar, such
as a `description: |` or `abstract: >` or anything a script writes, therefore costs its full length
on every repaint while any part of the row is on screen, and on every key near the block.

**Evidence.**
- `properties_view.cpp:330-334` (default branch: text, number, date) and `:302-303` (the key) lay
  out the full text. `:433` draws it with `drawText(value, kTextFlags, row.text)`, which wraps and
  lays out again on each paint. `:274` checks the time only when `rows_.size() % 16 == 0`. `:247-253`
  (`setLook`) lays out every row at once on a width or theme change.
- The Qt probe (`probe text N`) uses `title: x` and `description: |` with N lines of about 70
  characters, then a heading and a body:

  ```text
  N =   200  block 3,877 px; repaint with the block on screen: median 8.2 ms; typing in the body: median 8.6 ms
  N = 2,000  block 38,077 px; repaint: median 42.8 ms, max 49.6 ms; typing in the body: median 48.2 ms, max 56.0 ms;
             leaving the edited block: 146.7 ms on the key
  ```

- Brief section 4 asks for 16 ms from key to screen. The benchmark's hard budget is no key above
  50 ms. Story `:328-329`: "building and laying out follow the budget under 'Long front matter'".
- The benchmark's long cases are 5,000 short keys and one nested value. The nested value is JSON,
  so it is capped. No test or benchmark has a long text value.

**Ask.**
- Lay out text values once per width into a cached `QTextLayout`, or as `QStaticText`, and paint
  only the visible lines.
- Either show a long text value as a few lines with an ellipsis, as for JSON (and record that in
  the story), or split the layout of one row across slices.
- Add a benchmark key with a long block scalar: typing in the body below it, and leaving the edited
  block.

## O3 - failure - minor

**Claim.** The story says that "reading and building take at most 4 ms on a key and continue in
idle slices" (`:175-176`). Only building is sliced. `read()` copies every YAML line out of the
document and runs `parseProperties` to the end, whether it runs on the key that leaves the block,
in an idle slice, or on opening. `conceal()` and `reveal()` also walk every line in one go. The cost
of the leaving key therefore grows with the front matter without bound, and recognition itself has
no limit (decision 7).

**Evidence.**
- `properties_embedder.cpp:249-269` (`read`) has no time check. It is called from `work`
  (`:239`), which runs within the 4 ms of a key (`:279`), within a 6 ms idle slice (`:188`) and
  from `placeCaretBelow` (`:168`). `:318-348`: `conceal`/`reveal` loop over lines 1 to `close_`.
- `docs/performance.md:58,66` already gives 10–14 ms on the leaving key for 5,000 keys: about 4 ms
  of parsing and 2 ms of copying. That is above the story's 4 ms, but the text does not say so.
- Core probe: `parseProperties` takes 3 ms for 5,000 keys, 95 ms for 50,000 and 448 ms for 200,000
  (per line from about 500 ns to 2 µs, apparently from memory, not from the algorithm).
- Qt probe (`probe leave 50000`, about 1 MB of front matter): leaving the edited block takes
  130–159 ms on the key. The idle passes until the block is built take up to 45–55 ms each.
- Qt probe (`probe prose 120000`): a 10 MB CommonMark document that starts with `---` and has a
  setext underline at its end, which the story's known consequence (review O9) makes front matter.
  The longest event-loop pass while opening is 110–142 ms with the toggle on and 54–60 ms with it
  off (two runs each). The difference is one unsliced `read()` in an idle slice.

**Ask.** Either slice reading (copy and parse incrementally, or parse a snapshot off the UI
thread) and slice `conceal`/`reveal`, or change the story's rule to what the code does ("reading
is not sliced; building is") with the size from which the 50 ms budget no longer holds. Record the
numbers in `performance.md`.

## O4 - failure - minor

**Claim.** "After opening" puts the caret at the start of the line below the closing line, whatever
that line is. If it is the header of a table, `TableEmbedder` moves the caret into the table and the
first cell's editor takes the keyboard focus. The first key typed after opening a note then edits
the table header.

**Evidence.**
- `properties_embedder.cpp:170-172`: `setTextCursor(QTextCursor(below))` with no regard for the
  element on that line.
- Qt probe (`probe above`) with `---` / `a: 1` / `---` / `| x | y |` / `| - | - |` / `| 1 | 2 |`:
  after opening, `QApplication::focusWidget()` is `hitokoto::ui::CellEditor`. Typing `z` turns
  line 3 into `| zx | y |`. With a code fence or a thematic break below the front matter, the focus
  stays in the editor and the caret is on that line.
- Before S024 the caret stood at the start of the document (`document_session.cpp` at 3a17d89,
  `moveCursor(QTextCursor::Start)`). The story's own scenario "front matter directly followed by a
  table" (`:245-249`) does not open the file and check the caret.

**Ask.** Decide what opening does when an embedded element follows the front matter. For example,
leave the focus in the editor with the caret before the table, as a click above a table at the
document's edge does (S018), or keep the start of the document. Add the case to
`openingPutsTheCaretBelowTheFrontMatter`.

## O5 - failure - minor

**Claim.** Review O1 was resolved with "the full JSON is the row's tooltip and accessible name".
For the review records that motivated O1, the tooltip is a wrapped label of about 480 px width and
1,250–1,420 px height. Its top lies above the screen, so the full JSON cannot be read. Showing it
also takes tens of milliseconds, and 160 ms for a nested value of 5,000 lines.

**Evidence.**
- `properties_view.cpp:545-557`: `QToolTip::showText` with the whole JSON string.
- Qt probe (`probe tipfile`, offscreen screen 800×800):
  - `docs/reviews/023-thematic-breaks-code.md`, `objections` (6,079 characters): tooltip 481×1252
    at y -450, 24 ms;
  - `docs/reviews/024-front-matter-spec.md`, `objections` (6,667 characters): 481×1420 at y -618,
    43 ms;
  - `probe tooltip 5000` (72,787 characters): 114 ms for the event, 161 ms with the first events.
- A 1080 px screen cuts the first two tooltips as well.

**Ask.** Choose one:
- show a capped tooltip (for example the first 20 lines of pretty-printed JSON, then `…`) and rely
  on a click, which shows the YAML anyway, for the rest;
- or record in the story that the tooltip shows only what fits on the screen.

## O6 - failure - minor

**Claim.** Review O10 asked for badges that name the reason. The implementation commit 8ef87d7
widened "bad indentation" in the story's table to "any other text the subset does not expect".
Because of that, the errors writers make most often get an indentation reason that points them in
the wrong direction.

**Evidence.**
- Core probe:
  - `title: Note: draft` gives `Invalid YAML (line 2): bad indentation`;
  - `a: "C:\path"` (an unknown escape) gives the same;
  - `a: "x" y`, `a: [x] y` and `a: - x` give the same.

  ruamel.yaml names them "mapping values are not allowed here", "found unknown escape character",
  "expected <block end>" and "sequence entries are not allowed here".
- `properties.cpp:538-540`: `unexpected()` reports `Reason::BadIndentation`. It is called for
  `: ` inside a plain scalar (`:890-891`), for an unknown escape (`:1035-1037`) and after a quoted
  scalar or flow collection (`:699`).
- The story's table row (`024-front-matter.md:85`) was changed in 8ef87d7 ("as js-yaml names most
  of it"). The commit and the story do not record that Armin decided this.

**Ask.** Add two or three reasons, such as "colon in a plain value (quote it)", "unknown escape" and
"text after a value", or let Armin decide the catch-all explicitly. Keep one test per reason.

## O7 - failure - minor

**Claim.** The story says: "A click on a row puts the caret at the first character of that row's
value in the YAML (for a value on the lines below the key, on the first of them; for a null, at
the end of the key line)". Two kinds of value do not follow this rule:
- For a block scalar, the reported position is the `|` or `>` header on the key line, not the
  first character of the content on the line below.
- For a null followed by a comment, the position is directly after the colon, not the end of the
  line.

ADR 0007 says that S025 "builds on the lines that the parser reports for each key and value".

**Evidence.**
- `properties.cpp:750-753`: `valuePos = {i, v}` for any value on the key's line, and `v` is the
  column of the header.
- `properties.cpp:735`: `valuePos{i, key->colon + 1}` for an empty value.
- Core probe:
  - `a: |` / `  first line` gives value line 0, column 3, at the `|`;
  - `a:   # a note` gives column 2; the line is 12 characters long.
- The UI test clicks only on a plain value and on a sequence.

**Ask.** Report the first content line of a block scalar and the end of the key line for a null
(or the start of the comment). Alternatively, state in the story that a block scalar's value starts
at its header. Add both cases to the click test or to the core test of the lines.

## O8 - design - minor

**Claim.** Brief section 5 says that adding an element must not require editing existing elements
or the editor's core loop. Spec review O4 named two things a second embedder would have to change:
`TableEmbedder::sweep` and the wiring in `EditorWidget`. The disposition fixed only the sweep, with
`BlockLayout::Claimant`. The properties block is wired by hand into `EditorWidget` and
`DocumentSession`, so a third embedded element (an image, a math block) has to edit the same places
again. `block_layout.hpp:26` claims that "a new one registers and changes no other".

**Evidence.**
- `editor_widget.cpp` has 21 lines added for `properties_`:
  - seven signal connections (`:70-76`);
  - `applyTheme` (`:167-168`);
  - `resizeEvent` (`:230`, `:236`);
  - `scrollContentsBy` (`:244`);
  - `mousePressEvent` and a new `mouseReleaseEvent` (`:258`, `:264`), for this element only;
  - the key chain `tables_->handleEditorKey(event) || properties_->handleEditorKey(event)`
    (`:299`);
  - four accessors (`:397-409`).
- `document_session.cpp:184,241` call `placeCaretAfterOpening`, a rule of this element, from the
  session.
- `editor_widget.hpp:77`: `friend class PropertiesEmbedder`.

**Ask.** Choose one:
- an embedder interface registered in a list (caret moved, document changed, reposition, width,
  key, mouse held, after opening) that `EditorWidget` iterates;
- or amend ADR 0005/0006 and `architecture.md` to say that each embedded element is wired into
  `EditorWidget` and name the places.

## O9 - operational - minor

**Claim.** Four commits fix behaviour of other stories on this branch: S019 (geometry queries and
painting skip hidden blocks) and S023 (front matter closed in a later slice of a long file). S023
is merged into main (pull request #22). Neither story mentions the fixes. In the S023 code review,
O8 was resolved for the same situation by recording the S021 fix in S021.

**Evidence.**
- d5b7712/03d669e and 897478d/d24c0a8 (S019), 74e656b/60bddd8 (S023).
  `git diff f71cd38..HEAD --stat` shows no change to `docs/stories/019-*.md` or
  `docs/stories/023-*.md`.
- `docs/reviews/023-thematic-breaks-code.md`, O8: "the two S021 commits stay on this branch. S021
  records the fix and its tests under Implementation."
- CLAUDE.md: "A branch may hold several closely related stories; anything beyond that only after
  asking."
- The commits were reordered: d5b7712–d24c0a8 (dated 08:39–08:57) now come before c781444 (01:20).
  The intermediate trees were therefore not the ones tested at the time. This is no rule violation,
  only a reason to run the suite once on HEAD.

**Ask.** Record the fixes and their tests in S019 and S023, and Armin's approval to keep them on
this branch, as for S021.

## O10 - operational - minor

**Claim.** `docs/architecture.md:49` describes `PropertiesView` as "Each row is a child widget, so
it is an accessible element". The implementation deliberately avoids widgets per row: one widget
paints, and each row is a `QAccessibleInterface` registered on demand (`properties_view.hpp:4-6`,
`properties_view.cpp:80-131`, `:582-603`, and the commit message of c781444). The architecture doc
is the reference for the next embedded element, and this sentence describes the first version that
`performance.md:60-66` says was replaced.

**Ask.** Describe the row interfaces and their lifetime: registered on demand, deleted on `reset`
and in the destructor.

## O11 - operational - minor

**Claim.** The story lists the differences from the reference parsers that a differential run
found (`:341-348`) and says that the test values were checked against js-yaml. A few differences in
scalars are not listed. In these cases hitokoto follows the YAML 1.2 core schema and ruamel.yaml,
not js-yaml, and the story's JSON rule ("exactly as JavaScript's `JSON.stringify` writes the
value") is read against js-yaml.

**Evidence.** Core probe against js-yaml 4.3.2 (`CORE_SCHEMA`) and ruamel.yaml 0.19.1:

```text
a: -.5     hitokoto {"a":-0.5}  (number)   js-yaml {"a":"-.5"}   ruamel {"a":-0.5}
a: +.5     hitokoto {"a":0.5}   (number)   js-yaml {"a":"+.5"}   ruamel {"a":0.5}
a: 1e400   hitokoto {"a":null}  (number)   js-yaml {"a":"1e400"} ruamel {"a":null}
```

Everything else agreed with the references, apart from the differences the story lists and the
subset: 219 hand-made cases, 16,000 random front matters and 33 number formats, where all JSON
numbers matched Node.

**Ask.** Add these three cases to the list of differences and to `CasesFromTheDifferentialRun`, or
follow js-yaml.

## Explicitly not objecting to

- **The parser's correctness within its subset.** Values, duplicate keys (`1`/`01`/`"1"`,
  `null`/`~`), quoted escapes, folding, block scalars with chomping, flow collections and the line
  numbers in errors agreed with js-yaml and ruamel.yaml in 219 hand-made cases and in 16,000 random
  front matters, apart from the listed differences and O11. The ECMAScript number formatting
  matched Node in all 33 cases tried (`1e21`, `1e-7`, `5e-324`, `0x` beyond 2^64, `-0`, ...).
  Recursion is bounded by the depth limit, and deep nesting with 100,000 blank or comment lines
  stays linear (8–13 ms).
- **Byte-exact saving (ADR 0002).** Neither `PropertiesEmbedder` nor `PropertiesView` writes to
  the document; a click only moves the caret. `theFileStaysByteForByte` covers a BOM with CRLF,
  invalid YAML and the review record.
- **The `BlockLayout::Claimant` fix of review O4.** `TableEmbedder::sweep` asks before it undoes
  another element's state. Front matter above a table keeps its block through a table refresh, and
  `anotherElementsLinesSurviveATableRefresh` can fail without the check.
- **Replacing the document.** Opening another file, with or without front matter, while a 10 MB
  file with 200 properties is still being inserted in slices, and New after a shown block, left no
  hidden line, no reserved height and no stale rows (Qt probe `probe reopen`, `probe replaced`). The
  embedder keeps block numbers, not `QTextBlock`s.
- **Accessibility lifetime.** The row interfaces are deleted on `reset` and in the destructor,
  guard their view with a `QPointer`, and report themselves invalid once the row is gone. The styler
  holds the embedder only through a `QPointer`.
- **Selections and held buttons (review O6).** Shift+click, select all and a drag from the body
  across the block keep the block and move nothing. A selection from inside to below conceals the
  block, as specified. I suspected that a drag-and-drop of selected text might swallow the mouse
  release and leave `held_` set, but I could not reproduce it offscreen.
- **The S019 fixes themselves.** `blockBoundingRect` no longer lays out hidden blocks, and the
  painting of panels and decorations skips them. Both fixes are general and come with tests that
  were committed red.
- **Test-first order.** Every fix commit follows a commit with its failing test, and the S024 UI
  implementation commit adds no test changes.
