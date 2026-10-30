---
title: "Objection record: S023 thematic breaks"
mode: spec
spec: docs/stories/023-thematic-breaks.md
branch: agents/s023-thematic-break
commits_examined: "e1d011e..edfb851 (fe63a2d story, edfb851 red tests)"
date: 2026-10-30
dispositioned: 2026-10-30 (Armin decided O1-O3; O4-O12 and the capped concerns as proposed by Claude, before the red tests were committed)
reviewer: Advocatus Diaboli (independent agent)
evidence_tools:
  - commonmark.js 0.31.2
  - markdown-it-py 4.2.0 (commonmark)
  - cmark-gfm 0.29.0.gfm (Python cmarkgfm)
  - build/ci (Qt 6.12, offscreen); red tests run, see O3/O9
objections:
  - id: O1
    category: premise
    severity: major
    title: "`---` directly under text, the likely case of the request, stays plain text, and there is no setext story"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: setext headings are part of S023 (title, rules, scenarios, size L). Every line of a paragraph that ends in === or --- is a heading of level 1 or 2; the underline is concealed with its space kept and revealed from any line of its heading. Tests S023_ThematicBreaks.TextUnderlinedIsAHeading, EveryLineOfTheParagraphIsAHeadingLine, UnderlineIsRevealedFromItsHeading, DashesUnderAListItem; UI textUnderlinedIsAHeading, typingAboveABreakMakesAHeadingAndBack, typingAnUnderlineMakesTheLineAboveAHeading."
  - id: O2
    category: premise
    severity: minor
    title: "Front matter is called an open question, but the tests already decide it, with a half-rendered result for Obsidian notes"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: front matter is recognised (first line ---, closed by the next ---) and stays plain text; showing it as properties is the new story S024 (Open). Without a closing line CommonMark applies. Tests ClosedFrontMatterStaysText, FrontMatterOnlyAtTheStartAndClosed, UI frontMatterStaysText, S009 walk."
  - id: O3
    category: design
    severity: major
    title: "The lookbehind on paragraph text breaks the element-engine contract (brief §5, architecture.md) and is left to an implementation note instead of an ADR"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: ADR 0006 (Proposed) decides how context beyond one line travels: renderDocument as the whole-document reference, paragraph context in its own BlockState field with active() unchanged and cached in user state bits, a paragraph role declared by each line element (brief section 5 kept), setext found by a forward walk in slices, front matter at document start. The story requires the lazy UI rendering to equal the reference; UI test lazyRenderingEqualsTheReference. architecture.md follows when the ADR is accepted."
  - id: O4
    category: design
    severity: minor
    title: "'A break inside a list item is out of scope' contradicts tests that render breaks inside list items; where the line is drawn there is unspecified"
    disposition: fixed
    disposition_rationale: "Story: only a break on an item's marker line (- * * *) is a known deviation; breaks on continuation lines are in scope and drawn across the text column like every break. Tests in DashesUnderAListItem."
  - id: O5
    category: failure
    severity: minor
    title: "Constructs hitokoto does not render (ordered lists, block quotes, indented code, HTML, link reference definitions) count as paragraph text, so a break after them shows as text"
    disposition: fixed
    disposition_rationale: "Story: lines that start a block hitokoto does not render (ordered item, block quote, indented code, HTML, link reference definition) are no paragraph text where CommonMark lets them start, including the interruption rules (only a quote, an ordered item numbered 1 and HTML of types 1-6 interrupt a paragraph). HTML blocks beyond their first line and the parser disagreement on link reference definitions are listed as known deviations. Test DashesAfterUnrenderedBlocksAreABreak; Foo / 2. x / --- and Foo / [a]: /u / --- as setext in EveryLineOfTheParagraphIsAHeadingLine."
  - id: O6
    category: failure
    severity: minor
    title: "List membership across blank lines and indentation relative to the item are not modelled; the rule as written gives wrong results"
    disposition: fixed
    disposition_rationale: "Story defines the content column (1-4 spaces, 5 or more count as one, tabs to the next multiple of 4) and lazy continuation; list membership across blank lines and four-space indentation inside items are listed as known deviations. Tests -   foo / bar / --- and -\tfoo / --- as breaks, - foo / bar / --- inside the item as setext, -  / --- as a break (empty item)."
  - id: O7
    category: failure
    severity: minor
    title: "A long break line that wraps is unspecified: several empty visual lines with one centred rule"
    disposition: fixed
    disposition_rationale: "Story: a break that wraps keeps its height, gets one line centred in the block. UI test aWrappedBreakKeepsItsHeightAndGetsOneLine."
  - id: O8
    category: failure
    severity: minor
    title: "'Pixel-aligned' is promised but only checked at device pixel ratio 1; fractional Windows scaling is untested"
    disposition: fixed
    disposition_rationale: "Story: max(1, round(border-width x device pixel ratio)) device pixels, aligned to device pixel rows. UI test aConcealedBreakIsCrispAtTheDeviceScale, run again as tst_s023_thematic_breaks_scaled with QT_SCALE_FACTOR=1.5 (offscreen honours it, checked)."
  - id: O9
    category: operational
    severity: minor
    title: "The benchmark gate cannot see the new cost; the inherited content column makes one edit restyle a whole lazy paragraph"
    disposition: documented
    disposition_rationale: "Story, requirements: the benchmark gate gets a document with breaks, setext headings and long hard-wrapped and lazily continued paragraphs (s023_*: cold walk, typing on a break line, typing - before a long paragraph); it comes with the implementation, all budgets hold. ADR 0006 records the cost."
  - id: O10
    category: operational
    severity: minor
    title: "A third copy of the thematic-break recogniser, with different indentation semantics"
    disposition: documented
    disposition_rationale: "Story and ADR 0006: one shared thematic-break recogniser in the core used by bullet, table and S023, each caller deciding indentation; done with the implementation."
  - id: O11
    category: operational
    severity: minor
    title: "S003's accepted story text is not amended although its acceptance test changes"
    disposition: fixed
    disposition_rationale: "docs/stories/003-bullets.md notes that S023 changed its scope line; S023 says so in its header."
  - id: O12
    category: operational
    severity: minor
    title: "Collision with S022 (feature/links_references) in LineRender::empty(), theme.hpp and theming.md is not addressed"
    disposition: documented
    disposition_rationale: "Story, requirements: S022 touches LineRender::empty(), theme.hpp after the escape tokens, docs/theming.md and the backlog; whichever merges second keeps both."
---

# Objection record: S023 thematic breaks (spec)

Scope: `docs/stories/023-thematic-breaks.md` and its red tests (`tests/core/test_s023_thematic_breaks.cpp`,
`tests/ui/tst_s023_thematic_breaks.cpp`, `tst_s009_concealment::thematicBreaksRevealedOnlyUnderTheCaret`,
`S003_Bullets.NonItemsArePlainText`, stubs in `markdown.hpp`/`theme.hpp`/`theme.cpp`), read against
CLAUDE.md, docs/brief.md, docs/architecture.md, ADR 0002–0005, docs/theming.md, stories 002, 003, 009,
014, 018, 019 and 021, the code in `src/core` and `src/ui`, and the unmerged S022 branch.

I built and ran the red tests. 12 of 15 core tests fail, and so do 6 of 6 UI test slots plus the new
S009 slot, each at the `thematicBreak` flag, the theme stub or the drawn line, which are the expected
reasons. The three core tests that pass already (`OtherLinesAreNoBreaks`,
`DashesUnderParagraphTextAreASetextUnderline`, `InsideACodeBlockItIsCode`) pin today's behaviour as a
regression guard.

## O1 - premise - major

**Claim.** The story may leave the reported symptom in place for one of its two common forms, and the
"story of its own" it defers to does not exist.

**Evidence.**
- Basis (story line 5): "`---` alone on a line shows as text". The story does not record whether the
  `---` in Armin's documents follows a blank line or directly follows text.
- Story lines 23–26: "`Foo` / `---` … hitokoto does not render setext headings yet (a story of its own);
  such a line stays plain text." `docs/stories/README.md` lists no setext-heading story, and
  `grep -i setext docs/` finds the term only in S009 and S023.
- Scenario "editing the line above changes the break" (story line 70 ff.) and
  `tst_s023_thematic_breaks::typingAboveABreakTurnsItIntoTextAndBack` require that typing on the blank
  line above a rendered break turns it back into raw `---` in body text. Nothing tells the user that
  this is now an H2 underline. GitHub and Obsidian show `<h2>`; commonmark.js gives
  `"intro\nFoo\n---\n"` → `<h2>introFoo</h2>`.

**Why it matters.** Writing a sentence, pressing Enter and typing `---` is a very common way to add a
separator. After S023 that case looks exactly like the bug report, and an existing rule disappears as
soon as text is typed directly above it. The stated goal (story line 14) is that "a document is divided
visually the way it is when rendered". For `Foo` / `---` the story delivers neither the break nor the
heading.

**Ask.** Before confirming, have Armin confirm which form his report was about. Add the setext-heading
story to the backlog, ordered next to S023, and link it from line 25. Alternatively, take minimal setext
rendering (heading style on the line above) into this story.

## O2 - premise - minor

**Claim.** Front matter is presented as open, but `S023_ThematicBreaks.FrontMatterIsNotSpecial`
already decides it. The decided result looks worse than today for Obsidian notes, and the brief targets
Obsidian-style editing.

**Evidence.**
- Story lines 37–39: "hitokoto follows CommonMark until a story says otherwise (open question for Armin
  and Daniel)".
- `test_s023_thematic_breaks.cpp:169-173` asserts that line 0 of `---`/`title: x`/`---` is a break and
  that line 2 is not.
- `docs/brief.md:7`: "Obsidian-style hybrid live-preview". ADR 0002 decision 6 and S018 already made a
  compromise for files that Obsidian writes.
- Result per the story: the top `---` is drawn as a rule, `title: x` and the other properties are body
  text, and the closing `---` is plain text (commonmark.js: `<hr /><h2>title: x</h2>`). Today all four
  lines are uniformly plain.

**Ask.** Ask Armin and Daniel to decide at confirmation and record the decision in the story, rather
than confirming a story that contains an open question its own tests already answer.

## O3 - design - major

**Claim.** A break that depends on whether the line above is paragraph text, and on its list content
column, fits none of the engine's element kinds. The story pushes a cross-cutting change of the element
contract into an "implementation note" from a throw-away spike, without an ADR or interface design.

**Evidence.**
- `docs/brief.md` §5: "Adding a new element must not require editing existing elements or the editor's
  core loop." S003 made this an acceptance scenario ("no file of the heading element and no file of the
  editor loop changes, except the one registration line").
- `docs/architecture.md:29`: `BlockState` is "Opaque state from one line to the next while a
  multi-line element is open". `:83`: "Nothing else connects two elements."
- `markdown.hpp:152`: `LineElement` is "decided by one line". `:190-193`: a `MultiLineElement` "owns
  every line until it ends: line and inline elements do not run there". A paragraph state therefore
  cannot be a `MultiLineElement`, because it would suppress headings and bullets.
- The definition of "paragraph text" (story lines 27–32) lists headings, fences, code lines, table rows,
  breaks and setext underlines, plus the list content column. That is knowledge of every block element.
  Each future block element (block quotes, ordered lists, setext headings, HTML) must edit this rule,
  which is exactly what brief §5 forbids.
- Code that reads `BlockState::active()` as "a multi-line block is open":
  - `block_styler.cpp:128` (`nextLineFor` gives no table lookahead while active)
  - `:196` (code-panel margin)
  - `:275-276` (fence search, opening vs closing)
  - `fenced_code.cpp:97`
  - `cachedBlockState`/`cacheBlockState` (`:46-80`, BlockData only while active)

  If the paragraph state of the note (story lines 113–116, "can travel in `BlockState`") makes
  `active()` true, `nextLineFor` stops looking ahead after text. GFM `Foo\n| a |\n| - |` would then lose
  its table; cmark-gfm gives `<p>Foo</p><table>…`, so the table must stay.
- `ElementRegistry::blockStateAfter` (`element_registry.cpp:187-191`) runs only multi-line parsers
  today. Paragraph-ness requires running the line elements in that walk too, which changes the cost
  model that S021 tuned (see O9).

**Ask.** Make the "paragraph / container state" an explicit, documented interface decision before
confirmation: an ADR, and an update to `architecture.md` "Adding a Markdown element". It should say:
- which struct carries the state (a field next to `element`/`value`, with `active()` unchanged),
- how an element declares whether its lines are paragraph text and what content column it opens,
- what a future element must do, and how brief §5 is still met or why it is relaxed.

## O4 - design - minor

**Claim.** The scope statement and the tests disagree about breaks inside list items, and the drawing
of such a break is unspecified.

**Evidence.**
- Story line 40: "Out of scope: a break inside a list item (`- * * *`, example 61) stays a bullet with
  text."
- Tests that render breaks which CommonMark puts inside the item:
  - `test_s023_thematic_breaks.cpp:200` `{u"* foo", u"  ***"}` and `{u"- foo", u"", u"  ---"}` are
    expected to be breaks.
  - markdown-it-py: `'* foo\n  ***\n'` → `<ul><li>foo<hr /></li></ul>`; `'- foo\n\n  ---\n'` →
    `<ul><li><p>foo</p><hr /></li></ul>`.
- Scenario line 53: the line is drawn "across the width of the text". For a break inside an item it
  is not said whether the rule starts at the item's content column (as rendered HTML does) or at the
  text column. No UI test covers it.

**Ask.** Narrow the out-of-scope sentence to "a break on the marker line of an item". State that breaks
on continuation lines are in scope, and say where their rule starts and ends.

## O5 - failure - minor

**Claim.** Under the rule "paragraph text is a line that … not blank, not a heading, not a fence …"
(story lines 27–29), every construct hitokoto does not render yet counts as paragraph text. A `---`
after such a line is then plain text, where all three reference parsers draw a break. The story does not
list these as deviations.

**Evidence** (commonmark.js 0.31.2 = markdown-it-py 4.2.0 = cmark-gfm, unless noted):
- `1. foo\n2. bar\n---` → `<ol>…</ol><hr />`
- `> quote\n---` → `<blockquote>…</blockquote><hr />`
- `\n    code\n---` → `<pre><code>code</code></pre><hr />`
- `<!-- c -->\n---` → `<!-- c --><hr />`
- `[a]: /url\n---` → markdown-it `<hr />`; commonmark.js `<p></p><hr />`; cmark-gfm `<p>---</p>`. The
  reference parsers disagree here, which contradicts "agree on every case" (story line 43) once link
  reference definitions are in play.
- hitokoto's own table element already recognises these block starts (ordered list markers, `>`, four
  spaces, HTML) as "another block" (`table.cpp:87-130`, `startsOtherBlock`). S023's definition is less
  complete than code that already exists.

**Ask.** Either exclude these line starts from "paragraph text", for example by reusing the table
element's block-start test, or list them in the story as known deviations with the reason "shows text,
never hides".

## O6 - failure - minor

**Claim.** The rule "underline belongs to the item only if indented to the item's content column"
(story lines 30–32) cannot be evaluated from a per-line paragraph state. That state does not know
whether a paragraph after a blank line is still inside a list item. The absolute rule "four spaces or a
tab is no break" (line 20) also contradicts CommonMark inside items.

**Evidence** (markdown-it-py and commonmark.js agree):
- `- foo\n\n  bar\n---` → `<ul>…<p>bar</p></li></ul><hr />` (break), but `Foo\n\n  bar\n---` →
  `<p>Foo</p><h2>bar</h2>` (no break). After a blank line the two `  bar` lines look the same to the
  proposed state.
- `- foo\n    ***` → `<ul><li>foo<hr /></li></ul>` (4 spaces, a break inside the item);
  `- a\n  - b\n    * * *` → break in the nested item. Story line 20 and
  `test_s023_thematic_breaks.cpp:73` (`u"    ***"`, `u"\t---"`) make every such line text.
- The content column follows CommonMark's 1–4-space and ≥5-space rules and tab expansion:
  `-   foo\nbar\n  ---` → `<ul><li>foobar</li></ul><hr />`; `-\tfoo\n  ---` → break. The bullet
  element's marker span absorbs all trailing blanks (`bullet.cpp:61-65`). The story does not say which
  definition applies.
- None of these cases is in the tests.

**Ask.** Define which list container state is carried (across blank lines too), and how the content
column is computed. Otherwise, restrict the claim to "directly continued paragraphs" and list the other
cases as deviations.

## O7 - failure - minor

**Claim.** A long break line wraps, and the story only covers a single visual line.

**Evidence.**
- `editor_widget.cpp:52`: `setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere)`, so a line of
  80 or more dashes in a narrow window wraps.
- The concealed form keeps its space (`HideMode::KeepSpace`, scenario line 52), so the block keeps
  several empty rows.
- Line 53: "vertically centred in the line" does not say which line, or whether in the block.
- `test_s023_thematic_breaks.cpp:63` includes a 37-underscore break, but no UI test uses a wrapping
  width.

**Ask.** Specify the wrapped case, for example "centred in the block" or "the concealed break is one
line high", and test it at a narrow viewport. Note that the second option conflicts with "no change in
height on reveal" and would need a decision.

## O8 - failure - minor

**Claim.** "Pixel-aligned" (line 54) is a new promise. It is verified only at device pixel ratio 1,
while the request comes from Windows, where 125 % and 150 % scaling are common.

**Evidence.**
- `tst_s023_thematic_breaks.cpp:72-78` checks "one crisp row" in an offscreen grab.
- `grep -rn "devicePixelRatio\|QT_SCALE_FACTOR" tests src` finds nothing.
- `docs/theming.md:70`: px values "are device-independent pixels", and "Hairlines (1 px separators) are
  fixed".

**Ask.** Define pixel-aligned in device pixels: is a 1 px border one device pixel, or rounded logical
pixels? Add a test at a fractional scale factor, or drop the word from the scenario.

## O9 - operational - minor

**Claim.** "The benchmark gate must stay within the budgets" (lines 118–119) cannot detect an S023
regression, because no measured document contains what S023 adds. The required inheritance of the
content column also turns some single edits into a restyle chain as long as the paragraph.

**Evidence.**
- Corpora: `bench/core_bench.cpp:15-25` and `bench/hitokoto_bench.cpp:63-75` contain one-line
  paragraphs, each followed by a blank line, with no `---`/`***` and no lazy continuations.
  `S007_RenderAllLines10MB` renders every line with a default `LineContext{false}`, so no state is
  walked. `s021_codeblock_*` measures a walk inside one fence only.
- Inheritance is required by `{u"- foo", u"bar", u"---"}` being a break (`test_s023…:200`). The state
  after `bar` must carry the item's column 2.
- `block_styler.cpp:165-175`: a changed block state restyles the next line and calls `forgetExtent()`.
  Typing `- ` at the start of a long hard-wrapped paragraph therefore changes the state of every
  following line of it. In a hard-wrapped file without blank lines (S007 targets 10 MB) that means the
  rest of the document, in idle slices.

**Ask.** Add a benchmark document with breaks, setext underlines and long hard-wrapped and lazily
continued paragraphs. Measure a cold walk and the "type `- ` above a long paragraph" case. Record the
expected cost in `docs/performance.md`.

## O10 - operational - minor

**Claim.** S023 will add a third thematic-break recogniser. The two existing ones differ already, and
the story does not require a single shared one.

**Evidence.**
- `bullet.cpp:18-37` (`isThematicBreak(line)`) skips leading blanks of any width, so `      * * *` is
  "a break" to the bullet element (hence not a bullet), but not a break under story line 20.
- `table.cpp:60-73` (`isThematicBreak(line, start)`) leaves indentation to its caller.
- A new element-level recogniser would be the third copy, each with its own reading of indentation and
  tabs.

**Ask.** Require one shared core function (CommonMark 4.1 including the indentation rule), used by
bullet, table and the new element, with the S023 recognition tests as its contract.

## O11 - operational - minor

**Claim.** An accepted story's acceptance test changes, but the story text does not.

**Evidence.**
- `git diff origin/main` shows the change to `test_s003_bullets.cpp`, but
  `docs/stories/003-bullets.md:40` still reads "`---` (thematic break) … must simply stay plain text"
  (status "Done in v0.1").
- S021 sets the precedent of recording such a change (`021-code-block-fences-in-panel.md:6`: "which
  changed S014's accepted behaviour").
- S023 line 109 asserts that "the S003 story kept them as text while they were out of scope", but no
  note was added to S003.

**Ask.** Add a one-line note to S003 pointing at S023, as was done for S014 and S021.

## O12 - operational - minor

**Claim.** Daniel's unmerged S022 branch (`origin/feature/links_references`, order 23, "In review")
edits the same lines. The story does not mention merge order or the semantic merge.

**Evidence.**
- `git diff origin/main...origin/feature/links_references` changes `LineRender::empty()` (adds
  `links.empty()`). S023 changes the same expression (adds `!thematicBreak`) in `markdown.hpp:128-131`.
- Both insert after `EscapeTokens`/`escape`/`EscapeStyle`/`escapeStyle` in `theme.hpp`, and both add
  rows after `escape.marker` in `docs/theming.md`.
- A textual conflict in `empty()` resolved by taking one side silently drops the other's condition.
  `ConcealedLineIsHiddenWithItsSpaceKept` (`!r.empty()`) only catches it if S023's side is lost.

**Ask.** Name S022 in the story with the intended merge order. After the second merge, check that
`empty()` contains both conditions and that the theme documentation test still passes.

## Capped concerns

- ADR 0002 decision 6 (feature toggle, currently off): when enabled, it inserts a blank line before
  `foo` in `| 1 |`/`foo`/`---`. The `---` then underlines `foo` and loses its break, which contradicts
  `ABreakEndsATable`'s lazy-row case (cmark-gfm: with the blank line, `<h2>foo</h2>`).
- With `thematic-break.margin` > 0, typing the first character on the blank line above a break removes
  its margin, and everything below jumps by twice the margin. Not specified or tested.
- No test changes `thematic-break.margin` or `border` while a document is open. Both UI tests apply the
  theme before `open()` (`tst_s023…:168,191`).
- `OtherLinesAreNoBreaks` contains `u"- - -"` with U+00A0 (`test_s023…:74`), which looks the same as the
  break at line 63. Write it as `u"-\u00A0-\u00A0-"`.
- "Depends on" omits S021, although the Performance section relies on the S021 fence search and the S009
  slot puts a break after a code block.
- No UI test covers a break directly after an embedded table view (S018 reserved height); only the core
  test does.
- An empty item `- ` followed by `  ---` (CommonMark: `<ul><li><hr /></li></ul>`) is unspecified.
  `bullet.cpp` accepts `- ` as an item.

## Explicitly not objecting to

- The recognition rules of CommonMark 4.1 as listed in story lines 15–22, including example 60
  (`* * *` beats a bullet) and paragraph interruption. I re-ran all of them; the three parsers agree.
- The setext precedence cases the tests encode: `Foo`/`---`, `Foo`/`bar`/`---`, `Foo\`/`---`,
  `| a |`/`---`, `a | b`/`---` (cmark-gfm `<h2>`), `Foo`/`===`/`---`, the list cases in
  `DashesUnderAListItem`, and the S009 walk document. All match the reference parsers.
- Concealment with `KeepSpace` plus a drawn rule, and the revealed form as one marker run. Both keep
  line geometry stable on reveal, consistent with S009 and S021.
- The theme tokens and fallbacks (`colors.border`, 1, 0, `colors.marker`), ranges 0–8 and 0–64, and
  the margin as block geometry. They follow ADR 0003, ADR 0004 and the code-block precedent.
- Following GFM for a break that ends a table, as S018 already does. The table element already ends a
  table at a break (`table.cpp:119`).
- The test shape: `showAll` checks `render(...).blockState == blockStateAfter(...)` on every line.
  The UI tests check pixels, formats and geometry, and the red tests fail for the right reasons.
- Emphasis, escapes and bullets on break lines. Today they produce no output on any of the test lines
  (checked against `libhitokoto_core.a`), so `NoOtherElementClaimsABreak` asks for no new suppression
  mechanism.

## Disposition of the capped concerns

- ADR 0002 decision 6 toggle and `foo` / `---` under a table: documented in the story (known consequences).
- The margin disappears when typing turns a break into an underline: documented in the story (known
  consequences).
- Theme change while a document is open: `theMarginIsPartOfTheLayout` now changes the margin with the
  document open.
- U+00A0 literal in `OtherLinesAreNoBreaks`: written as `\u00a0`.
- "Depends on" now lists S002 and S021.
- Break directly after an embedded table view: covered by the core test `ABreakEndsATable` and by the
  table in `lazyRenderingEqualsTheReference`; no pixel test.
- Empty item `- ` followed by `  ---`: a break, test in `DashesUnderAListItem`.
