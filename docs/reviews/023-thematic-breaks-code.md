---
title: "Objection record: S023 thematic breaks (code)"
mode: code
spec: docs/stories/023-thematic-breaks.md
branch: agents/s023-thematic-break
commits_examined: "c6ae411.. the seven commits from \"S023: in progress\" to \"S021: a fence directly after a table opens the block\" (review copy in the cloud workspace: 6c121bb story update, b29db76 red tests, 7f02ddd core, 7f06928 UI, ba2b572 benchmark/fuzzer/docs, 1f97f00 S021 red tests, 022ddcc S021 fix; same trees as the commits in the repo)"
date: 2026-10-30
reviewer: Advocatus Diaboli (independent agent)
evidence_tools:
  - git log/show/diff of c6ae411..022ddcc (context e1d011e..c6ae411); git merge-tree against origin/feature/links_references
  - ctest on build/ci (Qt 6.12, Release, -Werror, offscreen): 173/173 passed
  - ctest -R "s009|s021|s023" on build/min68 (Qt 6.8.3): 32/32 passed
  - commonmark.js 0.31.2, markdown-it-py 4.2.0 (commonmark), cmark-gfm (Python cmarkgfm)
  - a core probe outside the repo (g++, renderDocument per-line classifier) with a random differential against commonmark.js (55,000 documents, three generators)
  - a Qt probe outside the repo linked against build/ci's libhitokoto_ui.a and libhitokoto_core.a (random edit sequences against renderDocument, key-to-repaint timings, caret walks); VM load about 2.3 on 2 vCPU from the background clang-tidy run
  - the implementer's benchmark runs (main1, main2, branch1-3; same VM)
objections:
  - id: O1
    category: design
    severity: major
    title: "Line elements cannot see the paragraph above, so ADR 0006's claim that brief section 5 holds is false for the next block elements"
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: failure
    severity: major
    title: "Typing or removing an underline under a long paragraph walks the whole paragraph synchronously on the key"
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: minor
    title: "Bullet lines that cannot interrupt a paragraph end it: an unlisted CommonMark deviation despite the story's differential claim"
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: failure
    severity: minor
    title: "Moving up inside a long setext heading conceals and re-reveals its underline on every step"
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: failure
    severity: minor
    title: "Front matter has no bound: a document that starts with a break loses all rendering down to the next `---` line"
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: operational
    severity: minor
    title: "The benchmark's 'typing on a break' types on a setext underline; its hard-wrapped paragraphs are all headings"
    disposition: pending
    disposition_rationale: null
  - id: O7
    category: operational
    severity: minor
    title: "The sliced walks of the styler run under no correctness assertion; UI-equals-reference is checked on one 27-line document"
    disposition: pending
    disposition_rationale: null
  - id: O8
    category: operational
    severity: minor
    title: "An S021 behaviour change rides on the S023 branch without its own branch, story note or recorded approval"
    disposition: pending
    disposition_rationale: null
  - id: O9
    category: cost
    severity: minor
    title: "Styling a 10 MB file got about 20 % slower; not recorded, and the new s023_* values are not compared by CI"
    disposition: pending
    disposition_rationale: null
---

# Objection record: S023 thematic breaks (code)

Scope: the implementation of S023 on `agents/s023-thematic-break`, commits c6ae411..022ddcc,
against `docs/stories/023-thematic-breaks.md`, ADR 0006 and the requirements the spec review left
as "documented" (O9, O10, O12). I read the core (`block_rules.*`, `element_registry.cpp`, the
elements, `markdown.hpp`), the UI (`block_styler.*`, `editor_widget.cpp`, `editor_formats.cpp`),
the benchmark, the fuzzer and the tests, ran the test suites on both Qt builds, and checked
behaviour with probes outside the repo: a per-line classifier of `renderDocument` compared with
commonmark.js over random documents, and a Qt probe that drives the real editor with random edits
and timed keys. Timings come from a loaded 2 vCPU VM and are indicative; the comparisons with a
control on the same machine are what matters.

## O1 - design - major

**Claim.** ADR 0006, decision 3 says that a new element "declares its own role and does not edit the
rule; brief section 5 holds". The interfaces do not support that. A line element's parse rule only
sees the line, and its paragraph role can only return a content column. CommonMark's interruption
rules (which blocks may start directly after paragraph text) and lazy-only paragraphs therefore live
in the registry and in `block_rules.cpp`, and only for blocks that hitokoto does *not* render. The
first rendered block element that needs either rule, such as an ordered list or a block quote, has
to change the registry or the interface, or it breaks S023 tests.

**Evidence.**
- `markdown.hpp:181`: `parse(std::u16string_view line)`, with no `BlockState` and no
  information about the line above. `markdown.hpp:188-193`: `paragraphColumn(line, match)` returns
  `std::optional<std::uint32_t>`, so it cannot express `BlockState::kLazyParagraph` (text that lines
  below continue lazily but never underline).
- `element_registry.cpp:189-199`: registered line elements are tried before
  `blocks::otherBlockStart(line, after)` (`:203-216`). Only that function knows `After::Paragraph` and
  `After::Lazy`. `block_rules.cpp`, `otherBlockStart`, hard-codes "only an ordered item numbered 1 with
  content interrupts a paragraph" and `OtherBlock::LazyText` for ordered items and block quotes.
- A future ordered-list `LineElement` registered with `add()` would claim `2. x` at `:189`
  regardless of the line above. Then `{Foo, 2. x, ---}`, which
  `S023_ThematicBreaks.EveryLineOfTheParagraphIsAHeadingLine` requires to be a setext heading, would
  become a break. A block-quote element could not return the lazy-only role, so
  `{> quote, bar, ---}` in `LazyTextAfterUnrenderedBlocks` would turn into a setext heading or into
  text.
- The flaw already shows in the bullet element, which cannot know that it follows paragraph text (O3).
- MultiLineElements do not help either: `matchBlock` passes them `state.value` or `0`, never the
  paragraph field (`element_registry.cpp:270-283`).

**Why it matters.** Brief section 5 requires that adding an element needs no edits to existing
elements or to the core loop. The accepted ADR states that this still holds, so the next stories will
plan against an interface that cannot carry their rules. The cost appears later, as an interface
change across all elements in the middle of another story.

**Ask.** Do one of two things. Give line elements the paragraph context they need: the `After` of
the line, and a role that can say "lazy only". Or amend ADR 0006 decision 3 and
`docs/architecture.md` ("Adding a Markdown element") to say that block elements with interruption or
laziness rules change the registry, and name which ones.

## O2 - failure - major

**Claim.** When an underline appears or disappears below a paragraph, the setext level is passed up
through every line of the paragraph synchronously, on the key, with no time budget. Typing `-` on
the line after a long hard-wrapped paragraph, or deleting it, costs time proportional to the
paragraph's length. Starting a bullet list under a paragraph does exactly that: `-` and `- ` are
underlines, and `- x` is no longer one.

**Evidence.**
- `block_styler.cpp:761-781`: `updateParagraphAbove` calls `propagateLevelUp(above, level)` from
  `styleBlock`. `propagateLevelUp` (`:740-759`) loops with `b.previous()` over every line whose level
  differs and has no timer. The downward walk, in contrast, stops after `kParagraphSearchNanoseconds`
  (`:621`), and the slices check their budget (`:701`, `:724`). `giveLevel` (`:679-681`) and
  `restartParagraphSearchesAfterEdit` (`:812-814`) contain further unbounded loops over the
  paragraph.
- UI probe (`a Qt probe outside the repo`, `probe underline N`). The document is `intro`, a blank
  line, N lines of hard-wrapped text, a blank line and `after`. With the caret on the blank line below
  the paragraph, the probe types `-`, waits until styling is complete, then presses Backspace.
  Key-to-repaint over three rounds:

  ```text
  N =  20,000   '-' 4.6-10.8 ms   Backspace  8.9-10.6 ms
  N = 150,000   '-' 37.1-37.4 ms  Backspace 37.5-44.3 ms   (about 9 MB)
  N = 230,000   '-' 59.8-67.0 ms  Backspace 52.6-66.6 ms
  control N = 150,000, 'x' on the same line (no underline): 1.6-6.1 ms
  ```

- Budgets: brief section 4 allows 16 ms from keystroke to screen, and the benchmark's hard budget is
  no key event above 50 ms (`kBudgets`). The story (`023-thematic-breaks.md:169-171`) says "all
  budgets hold". ADR 0006 decision 4 says the walk "runs within the synchronous budget and continues
  in idle slices".
- The benchmark gate cannot see this. Its 20,000-line paragraph is never underlined (O6).

**Why it matters.** Spec review O9 named this scenario: a hard-wrapped file without blank lines, at
the 10 MB size of S007. The cost scales linearly and breaks the hard 50 ms limit at about 230,000
lines on this VM. It is triggered by an ordinary action, starting a list under the paragraph.

**Ask.** Bound `propagateLevelUp` (and the loops in `giveLevel` and
`restartParagraphSearchesAfterEdit`) the same way as the walk: at most the synchronous budget on the
key, the rest in idle slices. The lines are restyled lazily anyway. Add an `s023_*` benchmark that
types `-` and then Backspace under the long paragraph.

## O3 - failure - minor

**Claim.** The story says that a differential run over 24,000 random documents "found only the
deviations listed here". A whole class is missing. The bullet element claims lines that CommonMark
treats as continuations of a paragraph: an empty `*` or `+` item, which may not interrupt a
paragraph, and a bullet indented by four or more columns after paragraph text. The paragraph ends
there, so a following `---` becomes a break, or `===` stays text, where CommonMark renders a setext
heading.

**Evidence.**
- Reference parsers (all three agree; no markdown-it or cmark-gfm difference is printed):

  ```text
  "Foo\n*\n---"                  cmjs: <h2>Foo*</h2>
  "Foo\n+\n---"                  cmjs: <h2>Foo+</h2>
  "Foo\n* \n==="                 cmjs: <h1>Foo*</h1>
  "Foo\n    - bar\n---"          cmjs: <h2>Foo- bar</h2>
  "- foo\n  bar\n  *\n  ---"     cmjs: <ul><li><h2>foobar*</h2></li></ul>
  ```

- hitokoto (`renderDocument`, core probe), in the same order: `. . HR`, `. . HR`, `. . .`,
  `. . HR`, `. . . HR`.
- Random differential (`gen3.py`, 5,000 documents of paragraphs, underlines, bullets and empty
  items): 246 documents differ from commonmark.js. 244 contain an empty `*`/`+` item or a 4-space
  bullet; the other 2 are the listed `-` / `  bar` deviation.
- Cause: `bullet.cpp:23-49` accepts a marker without text at any indentation (S003: "Indentation is
  treated as nesting depth"). The story's own definition (`023-thematic-breaks.md:39-40`) excludes
  every "list item marker line without text" from paragraph text, whatever comes before it. Neither
  the known deviations (`:72-87`) nor the tests mention this case.

**Why it matters.** The story's accuracy claim (`:91-95`) is what reviewers and S024 rely on. The
case is easy to reach while typing: the first `*` of a list under a line of text, followed by an
existing `---` below.

**Ask.** Either list it as a known deviation and correct the claim, or let the bullet's paragraph role
follow CommonMark 5.2 after paragraph text (which depends on O1), with core tests for the five
inputs above.

## O4 - failure - minor

**Claim.** Inside a setext heading longer than the 2 ms walk, each caret step upwards starts a new
walk down to the underline. It does not stop at the lines already known to continue the paragraph.
While the walk runs, `setRevealed` receives `-1`, so the underline is concealed and then revealed
again once the walk ends in idle slices: it flickers on every step.

**Evidence.**
- `block_styler.cpp:829-849`: the cache answers only for `number >= underline_.first`. Otherwise a
  new search starts at `number + 1` (`:843`) and `continueUnderlineSearch` (`:856-872`) walks every
  line down to the underline. `setRevealed` (`:494-513`) sets `revealedUnderline_ = -1` and restyles
  the old underline as concealed; `underlineSearchSlice` (`:877-881`) reveals it later.
- UI probe, `probe caretup 150000 75000` (a 150,000-line heading, caret on line 75,000, 20 presses of
  Up): right after the key the underline was not revealed in 10 of 20 steps, and the idle work per step
  averaged 55 ms. Starting from the last line of the heading it was revealed after 20 of 20 steps.
- Story S009 scenario (`023-thematic-breaks.md:154-159`): "an underline is revealed exactly while the
  caret is on a line of its heading".

**Why it matters.** It only affects headings longer than what the 2 ms walk covers, a few thousand
lines. That is the same long-paragraph family as O2 and O9 of the spec review, and the visible
symptom is flicker.

**Ask.** End the walk when it reaches `underline_.first` with the same `inside` state, or keep the
previous underline revealed while a search for the same paragraph runs. Add a UI test with a heading
longer than the synchronous budget.

## O5 - failure - minor

**Claim.** Front matter is any content between a first-line `---` and the next line `---`, however far
apart and whatever lies between. A CommonMark document that starts with a thematic break and later has
a setext heading underlined with `---` is shown as plain text down to that underline: no headings, no
bullets, no break.

**Evidence.**
- `front_matter.cpp:35-44`: inside, every line is `BlockLine{kInside, false}` until a delimiter.
- Core probe: `---` / `Intro paragraph.` / empty / `# Chapter` / `- item` / empty / `Section` / `---` /
  `body` renders all 9 lines as plain text (no heading, break or decoration). commonmark.js gives
  `<hr /><p>Intro paragraph.</p><h1>Chapter</h1><ul><li>item</li></ul><h2>Section</h2><p>body</p>`.
- ADR 0006 decision 5 speaks of a search "bounded by the front matter's own length". For unclosed or
  far-closed front matter that length is the whole document; `frontMatterAfterEdit`
  (`block_styler.cpp:969-997`) repeats the walk on every edit of the first line.
- S024 (`024-front-matter.md`, "To refine") leaves invalid YAML open but fixes "Recognition stays as
  in S023". The S023 tests (`ClosedFrontMatterStaysText`, `FrontMatterOnlyAtTheStartAndClosed`,
  `frontMatterStaysText`) only use well-formed three-line front matter.

**Why it matters.** The decision (spec review O2) was made with Obsidian notes in mind. Its failure mode
for plain CommonMark documents is silent: the text looks unrendered, with no cue as to why.

**Ask.** Armin decides whether recognition needs a limit (for example no blank line inside, or only
lines that can be YAML), or records the consequence in the story as a known deviation. Add a test for
a document that starts with a break and has a setext heading later.

## O6 - operational - minor

**Claim.** The story requires a benchmark of "typing on a break line". `measureBreakTyping` types on a
setext underline instead, because every `---` in the benchmark document underlines the twelve
hard-wrapped lines above it. Those "hard-wrapped paragraphs" are therefore all level-2 headings. The
document's only real breaks are `***` and `___`.

**Evidence.**
- `bench/hitokoto_bench.cpp:109-116`: a section is `Section i` / `-------` / blank / twelve text lines
  / `---` / `- an item…` / … / `***` / blank / `Text directly above a break` / `___`.
- Core probe over two sections: the twelve text lines are `H2` and the `---` after them is `U2`. Both
  lines that read exactly `---` are underlines.
- `:398-404`: the measurement picks the first block whose text is `---` from the middle of the
  document. The comment at `:400` says "the line changes between a break and paragraph text on every
  key", which is not what happens: it changes between an underline and paragraph text.
- Nothing types an underline under the 20,000-line paragraph (O2).

**Why it matters.** The `s023_break_*` values cover a different code path (level propagation over 12
lines) than their name and the story claim. A regression in typing on a break, or the O2 cost, would
go unnoticed.

**Ask.** Put a blank line before the section's `---`, or measure on `***`/`___`. Rename or keep the
underline case as its own measurement, and add the underline-under-a-long-paragraph case (O2).

## O7 - operational - minor

**Claim.** ADR 0006 requires the lazy UI rendering to equal `renderDocument` for every line. The only
check is one 27-line document, with 8 caret positions and 3 edits, all after `settle()`. No UI test
has a paragraph, heading or front matter long enough to leave the synchronous budget. The idle-slice
paths therefore never run under a correctness assertion: `paragraphSearches_`,
`applyParagraphLevel`, `restartParagraphSearchesAfterEdit`, the underline search slices and the
front-matter slices (`block_styler.cpp:684-827`, `:877-882`, `:963-967`). The benchmark runs some of
them but measures only time.

**Evidence.**
- `tests/ui/tst_s023_thematic_breaks.cpp:294-369` (`lazyRenderingEqualsTheReference`), and the S009
  walk document `tst_s009_concealment.cpp:127-145` (18 lines).
- The synchronous budgets are 2 ms for the walk and 4 ms for front matter (`block_styler.cpp:44`,
  `:925`). About 0.3 µs per line means more than 6,000 lines.
- Feasibility: my probe (`probe diff`, `probe bigdiff`) compares every line with `renderDocument`
  after random keys. It covered 210 small documents with 30 edits each (with and without settling
  between keys), and 14 documents with front matter, a 30,000-line paragraph under a bullet and a
  15,000-line setext heading, 12 edits each. It found **0 mismatches** in a few minutes.

**Why it matters.** The styler's correctness now rests on cached bits (`kLevelKnown`, links, paragraph
column) and on four timers that interact. Merging S022 and implementing S024 will touch it, and the
current test would not catch a stale level in a long paragraph.

**Ask.** Commit a seeded random-edit differential as a UI test: small documents, plus one document
past the synchronous budgets, compared with `renderDocument` after every settle.

## O8 - operational - minor

**Claim.** Commits 1f97f00 and 022ddcc change S021 behaviour for every code block (how an opening fence
is told apart, margins, fence reveal) on the S023 branch. S021's story is not amended, and nothing
records that the bundling was agreed.

**Evidence.**
- `022ddcc` changes `fenced_code.cpp` (language when `parsed.stateAfter != 0`) and
  `block_styler.cpp` `applyBlockSpacing`/`codeExtentOf` (opening means the state after the line is
  active, instead of the state before it being inactive).
- `CLAUDE.md`: "A branch may hold several closely related stories; anything beyond that only after
  asking". Precedent: S021 itself records "changed S014's accepted behaviour"
  (`021-code-block-fences-in-panel.md:6`).
- `git diff --stat c6ae411..HEAD` shows no change to `docs/stories/021-code-block-fences-in-panel.md`.

**Why it matters.** The fix itself looks right (see below), but it would ship under S023's review and
PR. S021's record would not show that a fence directly after a table now opens a block, with a margin
and a badge.

**Ask.** Get Armin's explicit OK for keeping it on this branch, or move it to its own `agents/…`
branch. Either way, add a line to S021 naming the change and the tests
(`AFenceDirectlyAfterATableOpensABlock`, `aFenceDirectlyAfterATableOpensTheBlock`).

## O9 - cost - minor

**Claim.** Styling a whole 10 MB file became measurably slower. This is not recorded anywhere, although
spec review O9 asked for it and ADR 0006 only says "somewhat slower". In addition, CI compares none of
the new `s023_*` values with a baseline.

**Evidence.**
- The implementer's runs on this VM (main and branch run in alternation):

  ```text
                         main1    main2    branch1  branch2  branch3
  open_10mb_styled_ms   3572.5   3469.2   4311.4   4261.8   3862.6
  s018_table_open_ms     130.8    128.6    153.7    140.1    147.3
  ```

  For `open_10mb_styled_ms` the median rises from about 3,520 to 4,262 ms, +21 %, with a range of +8 %
  to +24 %. That is close to the 25 % Linux tolerance, and this value is compared, not report-only.
- `git diff --stat c6ae411..HEAD -- docs/performance.md bench/baselines` is empty.
- `bench/baselines/linux.json` has no `s023_*` keys. The comparison loop iterates baseline keys only
  (`hitokoto_bench.cpp:724-739`), so CI gates S023 only through three hard budgets until the baselines
  are updated. That is the S021 precedent, but the story says the gate "gets" these measurements.
- The new 10 MB document is opened 5 times (`medianOpen`), about 4 s of styling each. That adds roughly
  half a minute to each benchmark job.

**Why it matters.** A 20 % regression in the background styling of large files is a real cost of ADR
0006. If it is not recorded, the next story that adds 5 % fails CI with no history to explain it.

**Ask.** Record the S023 measurements and the per-line walk cost in `docs/performance.md`. State
whether the slowdown is accepted. Plan the `s023_*` baseline update from the first CI run, as for
S021.

## Explicitly not objecting to

- **Recognition and precedence in the core.** Across 55,000 random documents (three generators), every
  difference from commonmark.js that I found is either a listed known deviation or the O3 class. This
  covers thematic breaks, setext underlines at every indentation, multi-line paragraphs, lists with
  content columns and tabs, lazy lines and the table cases (`| a |` / `---` is setext; a table followed
  by `***`, `- - -`, a lazy row and `---` ends in a break, as in cmark-gfm).
- **Lazy rendering equals the reference.** My random-edit probe found no mismatch, including the
  idle-slice paths. O7 asks only that such a check be committed.
- **The shared recogniser (spec review O10).** `block_rules.hpp` is the only thematic-break recogniser;
  bullet and table use it, and each decides indentation.
- **S022 (spec review O12).** `LineRender::empty()` contains `!thematicBreak && setextUnderline == 0`,
  and the theme tokens are where O12 expects them. `git merge-tree` with
  `origin/feature/links_references` shows 11 conflicting files, all of them textual overlaps of the
  kind O12 anticipates.
- **The S021 fix itself (022ddcc).** Telling an opening fence by "the state after the line is active"
  is correct for fences after tables, at the end of the document and for back-to-back blocks; only its
  placement is objected to (O8).
- **The drawn line and the theming baseline.** Device-pixel thickness and alignment, the line for
  wrapped breaks, margin in the geometry, four tokens with fallbacks and documentation. The UI tests,
  including the 1.5 scale run, pass on Qt 6.12 and Qt 6.8.3.
- **Extra render calls while painting.** `paintThematicBreaks` adds one `renderFor` per visible line,
  and setext heading lines render twice in `renderLine`. This is bounded by the lines on screen.
