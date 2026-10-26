---
mode: code
spec: docs/stories/013-inline-emphasis.md
commit_range: 2d9318a..5d0a142  # S013-relevant: cd135a2, 9504915, d5b99b4, 787ae49 (PR #2), 3e2b072, 5d0a142
head_examined: 5d0a142 (agents/implement-markdown-codeblocks)
date: 2026-10-25
dispositioned: 2026-10-26 (branch agents/s013-objections, on top of main bdfd255)
objections:
  - id: O1
    category: design
    severity: minor
    disposition: fixed
    disposition_rationale: Fixed by Daniel in 89a7dda (PR #6) before this branch; a run closes or opens nested pairs of different length. Test fb1a1c4.
  - id: O2
    category: design
    severity: minor
    disposition: documented
    disposition_rationale: Registration order is the contract; addInline and docs/architecture.md say so (6e60485). A mechanism without an order is not worth its cost for a minor.
  - id: O3
    category: design
    severity: minor
    disposition: fixed
    disposition_rationale: The registry tells inline runs apart by position, the list of inline roles is gone (93e5e09). Test 1b4821e.
  - id: O4
    category: failure
    severity: major
    disposition: fixed
    disposition_rationale: Nearest opener plus CommonMark's rule of 3 (92ca3a9). The objection's second example is wrong, see below. Tests 25684a0, 3aabbbd.
  - id: O5
    category: failure
    severity: minor
    disposition: fixed
    disposition_rationale: Inline-code delimiters are skipped like code text (6f5d39b). Test d7280f0 adds a backtick to the property test's alphabet.
  - id: O6
    category: failure
    severity: minor
    disposition: fixed
    disposition_rationale: Bold is never lighter than the heading (c14f15b). Visibility with the built-in themes is a theme decision, documented in the story and docs/theming.md. Test 77d7b51.
  - id: O7
    category: operational
    severity: minor
    disposition: resolved
    disposition_rationale: Status and CI run were already corrected on main (fb15427, 85af5c6); "Completed" reworded to "Implemented" (6e60485).
---

# Review S013, mode code: objections and dispositions

The objections below are the Advocatus Diaboli record of 2026-10-25 as delivered, examined at
5d0a142. The dispositions were decided by the project owner on 2026-10-26 and implemented on the
branch `agents/s013-objections`, test first: every test was committed red on main 55c5b08 before
the fixes, then rebased onto main bdfd255 (which already carried Daniel's O1 fix, 89a7dda).

Expected renderings were checked against commonmark.js 0.31.2 and markdown-it (CommonMark mode).

## O1 - design - minor

The same-length pairing rule silently breaks a common nesting form. `**bold *italic***` and
`*italic **bold***` render no emphasis at all, and every delimiter stays visible. The closing run
`***` has length 3, and `matchDelimiters` pairs only identical marker and length
(`src/core/src/elements/emphasis.cpp:116-117`). Scope line 58 states the restriction, but the brief
names Obsidian as the model (`docs/brief.md:7`), and Obsidian renders both forms. The scenario
"combined and nested emphasis" (story line 19) only lists examples that avoid the case. Either
split closing runs (a `***` closer serving `*` and `**` openers) or name this form explicitly as
unsupported in the story.

**Disposition: fixed.** Daniel fixed it in 89a7dda (PR #6, merged into main before this branch):
a run is split so that it closes, or opens, nested pairs of different length. Test
`DelimiterRunClosesOrOpensNestedPairsOfDifferentLength` (fb1a1c4) covers both forms and
`***bold** italic*`; Daniel's own tests cover `_` and `~~`. Known remaining deviation from
CommonMark: `***a*b***` (CommonMark takes delimiters from the inner side of the opening run);
not a form anyone writes on purpose.

## O2 - design - minor

Emphasis depends on inline code having run first, and nothing but the registration order in
`withBuiltins()` guarantees that. `EmphasisElement::render` reads `Role::InlineCode` runs out of
`out.runs` to exclude code characters (`emphasis.cpp:134-139`). Inline code is registered before
emphasis (`element_registry.cpp:198-199`), but `addInline` documents no ordering
(`markdown.hpp:193`). `docs/architecture.md:75` claims "no element has to know the others", and
emphasis knows inline code. Register the two in reverse order and `*` inside code spans gets
emphasized and concealed.

**Disposition: documented.** Registration order is the contract: `addInline` now says that inline
elements render in registration order and see the runs of the earlier ones, and
`docs/architecture.md` says the same instead of the claim quoted above (6e60485). A mechanism that
removes the order dependency (declared exclusions, priorities) would cost more than a minor
objection justifies; the order is set in one place, `withBuiltins()`.

## O3 - design - minor

To add the emphasis role, the registry's core loop had to be edited. `isInlineRole` hard-codes
`InlineCode`, `InlineCodeMarker` and `EmphasisMarker` (`element_registry.cpp:10-12`, extended for
emphasis in 3e2b072). This contradicts the brief: "Adding a new element must not require editing
existing elements or the editor's core loop" (`docs/brief.md:69`). Every future inline element
with a revealed marker (links, highlights) will need the same edit.

**Disposition: fixed.** The registry remembers how many runs the block element produced and
treats every run appended after that as an inline run; `isInlineRole` is gone (93e5e09). Test
`InlineElementsAreAddedByRegistrationOnly` (1b4821e) registers a test-only highlight element whose
markers reuse the `Marker` role and checks that the heading run is split around them.

## O4 - failure - major

One unmatched delimiter inside a valid pair cancels the whole pair. `**2*3 is six**` and `**a *b**`
render without any bold, and every marker stays visible. In CommonMark and Obsidian, both are bold.
Cause: `matchDelimiters` only checks the top of the opener stack. A non-matching delimiter that can
also open is pushed (`emphasis.cpp:116-123`), and from then on it blocks the outer closer. The
scenario "non-matching delimiters stay literal" (story lines 31-35) only says that the *unmatched*
pair gives no emphasis, not that it cancels neighbouring pairs. The core test only covers lines
without any valid pair (`tests/core/test_s013_inline_emphasis.cpp:135-140`). Reproduce with a
probe linked against `src/core`:
`registry.render(u"**2*3 is six**", {false})` gives empty `hidden` and empty `inlineStyles`.

**Disposition: fixed, with a correction to the objection.** `**2*3 is six**` is bold in CommonMark
because of the rule of 3: a run that can both open and close does not pair with a run when the two
run lengths add up to a multiple of 3. `**a *b**` is *not* bold in CommonMark
(`*<em>a <em>b</em></em>`, commonmark.js and markdown-it agree), so that example was dropped. After
Daniel's O1 fix the failure had changed shape: the inner `*` paired with part of the closing run
(`**2*3 is six**` rendered "3 is six" italic). `matchDelimiters` now takes the nearest earlier
opening that fits and that the rule of 3 allows, and discards the openings it skipped (92ca3a9).
Tests 25684a0 and 3aabbbd: `**2*3 is six**`, `**x*y**`, `*2**3*`, `**a*b*c**`.

## O5 - failure - minor

Emphasis styles cover the backticks of inline code. That breaks the registry contract (styled
ranges and hidden ranges never overlap, `element_registry.cpp:52`) and overrides the
`inline-code.marker` token. `codeCharacters` only marks `Role::InlineCode`, not
`InlineCodeMarker` (`emphasis.cpp:135-136`). For ``**`code`**`` the probe returns
`inlineStyles [2,3) [7,8)`, exactly on the hidden backticks `[2,3) [7,8)`. On the revealed line,
`EditorFormats::ranges` then makes the backticks bold and paints them in `emphasis.bold-text`
instead of `inline-code.marker`. Only `role != InlineCode` is excluded (`editor_formats.cpp:136-155`).
The S013 property test asserts this very invariant, but its alphabet contains no backtick (`test:111`).

**Disposition: fixed.** `InlineCodeMarker` runs are excluded like `InlineCode` runs (6f5d39b).
Test `EmphasisLeavesInlineCodeDelimitersAlone` and a backtick in the property test's alphabet
(d7280f0); the property test failed on 55c5b08 exactly as the objection predicted.

## O6 - failure - minor

Bold inside headings is invisible with the built-in themes. The markers get concealed, but the text
looks unchanged. Both built-in themes set `headings.weight = 700` (`theme.cpp:491`, `534`), and
`emphasis.bold-weight` also falls back to 700 (`theme.cpp:666`). `ranges()` replaces the heading
weight with the bold weight (`editor_formats.cpp:52`, `139`). With `headings.weight` above
`bold-weight`, bold text even turns lighter than its heading. `EmphasisIsAppliedInsideHeadings`
(`test:100-107`) only checks core ranges, not the visible result.

**Disposition: fixed in part, rest documented.** The defect (bold lighter than its heading) is
fixed: the bold weight applies only when it is above the surrounding weight (c14f15b); UI test
`boldInsideHeadingIsNeverLighterThanTheHeading` (77d7b51) with `headings.weight = 800`. Whether bold
inside headings should be visible with the built-in themes is a theme decision: Obsidian with its
default settings shows no difference either, and changing the built-in heading weight would change
every heading. Left to a later theme decision; recorded in the story's scope and in
`docs/theming.md`, which says how a theme makes it visible.

## O7 - operational - minor

The story contradicts itself on completion. The header says "In progress (CI pending)"
(story line 3), the section "Implementation" says "Completed 2026-10-25. Core and UI tests pass on
Linux" (line 71). CLAUDE.md and `docs/stories/README.md:33` require green CI on all three OSes
before "In review", and README line 38 says the CI "has not run yet". PR #2 is already merged into
`main` (787ae49). "Completed" invites reading the story as done before Windows and macOS were
checked even once.

**Disposition: resolved.** Before this branch, fb15427 and 85af5c6 set the story to "In review"
and recorded CI run #18 (green on Linux, Windows and macOS). "Completed" is now "Implemented"
(6e60485); "Done" stays the user's call.

## Explicitly not objecting to

- The focused inline subset instead of full CommonMark delimiter resolution: the scope declares
  it (story line 58), except for the case named in O1.
- Concealing via a 1 px transparent font (ADR 0001); S009 decides on it as a standing quality gate.
- Text shifting horizontally when the caret enters a line: that is the intended Obsidian-style
  reveal.
- ASCII-only punctuation in the flanking rules: German quotation marks like `„**Wort**“` and
  `**„Zitat“**` render correctly.
- Inline code inside emphasis staying unbolded: a consistent rule ("inline code takes
  precedence", `architecture.md:61`).
- Runtime cost: allocations per line only on lines with `*_~`; measured and documented in
  `docs/performance.md:44-47`.
- Byte-exact source: the UI tests check `toPlainText()` against the input
  (`tst_s013_inline_emphasis.cpp:61`, `102`).
