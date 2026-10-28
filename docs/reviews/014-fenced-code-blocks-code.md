---
mode: code
spec: docs/stories/014-fenced-code-blocks.md
branch: fix/block-comments-backticks-and-margin  # Daniel's branch, one commit
commit_range: e997683..b00ea24
reviewed_on: 2026-10-28
dispositioned: 2026-10-28 (branch agents/s014-fences-in-block from main 7f1a163, failing tests in 2b4386c)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), presets dev and ci; merge with main 759a59d
objections:
  - id: O1
    category: premise
    severity: major
    title: S014 (Done) rewritten in place instead of a new story
    disposition: fixed
    disposition_rationale: "Daniel's change is story S021 with its reason; S014 keeps its accepted text and points to S021. Confirmation of S021 with Daniel is open."
  - id: O2
    category: failure
    severity: major
    title: Every caret step walks the whole code block; cost grows linearly with its size
    disposition: fixed
    disposition_rationale: "The fences are found by walking cached block states once per block (4 ms at most per key, the rest in idle slices) and reused while the caret moves and types inside; 10 MB block, caret step max 108.8 -> 26.4 ms, typing median 72.4 -> 1.9 ms; tests aCaretStepInALargeBlockRendersFewLines, enteringALargeBlockRendersAndRestylesFewLines, benchmark s021_codeblock_*."
  - id: O3
    category: design
    severity: major
    title: paintCodeBlockPanels moves QTextLines while painting; BlockLayout owns the geometry
    disposition: fixed
    disposition_rationale: "BlockLayout::setBlockSpacing puts the margin above the opening and below the closing fence line, part of the block height in the height index; painting moves no line; tests theMarginIsPartOfTheLayout, paintingMovesNoLine, thePanelOverlapsNoNeighbour."
  - id: O4
    category: operational
    severity: major
    title: Language badge and the 3 px panel offset have no theme tokens
    disposition: fixed
    disposition_rationale: "Tokens code-block.margin (3), badge-text (code-block.marker), badge-font-family (editor font), badge-font-scale (1) in docs/theming.md; core tests S021_CodeBlockFences.ThemeTokens*, UI test theBadgeFontComesFromItsTokens."
  - id: O5
    category: operational
    severity: minor
    title: The vertical part of code-block.padding no longer applies
    disposition: documented
    disposition_rationale: "With the fence lines inside the panel they are the vertical space between panel edge and code; code-block.padding is horizontal, documented in docs/theming.md and S021. Vertical padding on top would make the panel two padding heights taller than b00ea24."
  - id: O6
    category: premise
    severity: minor
    title: The badge shows the whole info string instead of the language
    disposition: fixed
    disposition_rationale: "The badge shows the first word of the info string with backslash escapes resolved, as commonmark.js 0.31.2 and markdown-it-py emit it; entity references stay as written (S021). Tests S021_CodeBlockFences.Language*, theBadgeShowsTheLanguage."
  - id: O7
    category: design
    severity: minor
    title: A second fence parser in the UI
    disposition: fixed
    disposition_rationale: "Core reports LineRender::codeLanguage for an opening fence; the editor's own fence parser is gone."
  - id: O8
    category: operational
    severity: minor
    title: Not test-first
    disposition: resolved
    disposition_rationale: "b00ea24 cannot be redone test-first; its rework is: failing tests in 2b4386c, then one commit per objection."
  - id: O9
    category: operational
    severity: minor
    title: S009 not extended for the new concealment
    disposition: fixed
    disposition_rationale: "S009 has a scenario for code block fences; tst_s009_concealment walks the caret through a document with headings, bullets, emphasis, inline code and code blocks (fences revealed, block still, caret never in concealed text)."
---

# Review S014, mode code: objections and dispositions

Advocatus Diaboli pass over Daniel's commit b00ea24 on `fix/block-comments-backticks-and-margin`
("reveal code block fences when cursor is inside block and add margins"), on 2026-10-28. The
branch starts at e997683, before S018/S019; the merge with main 759a59d conflicts only in the
declarations of `src/ui/block_styler.hpp`. Branch and merge build with the preset ci without a
warning, 109 and 130 tests pass, clang-tidy over src, tests and bench finds nothing, and the
benchmark gate passes locally. The benchmark has no large code block, so it cannot see O2.

The commit changes how code blocks look and behave: concealed fences keep their line height,
the fence lines sit inside the panel, the opening fence shows a language badge, a caret anywhere
in the block reveals both fences, and the panel is moved 3 px down.

## O1 - premise - major

S014 is Done (accepted by the user on 2026-10-27). The commit rewrites two of its scenarios
("fence lines do not appear as panel content", "source is revealed on a fence line") instead of
adding a new story. The workflow (CLAUDE.md, `docs/stories/README.md`) makes scope changes new
stories with their reason; an accepted story's text is the record of what was accepted.

**Disposition: fixed.** The change is story S021 (`docs/stories/021-code-block-fences-in-panel.md`)
with its reason: a collapsing fence made the block jump on every entry and exit; Obsidian keeps
it still. S014 is back to the text accepted on 2026-10-27 and names S021 as the story that changes
it; the backlog lists S021 as Proposed. Daniel's new test moved to `tst_s021_code_block_fences`.
His adjustments in `tst_s014_fenced_code_blocks` stay, they test what S021 changes. Open: Daniel
confirms S021 (story text, margin and badge tokens, horizontal padding, O5).

## O2 - failure - major

`BlockStyler::setRevealed` calls `expandCodeBlocks`, which walks from the caret line to both
fences and renders every line on the way, on every caret move. The revealed range then spans the
whole block, so entering or leaving it restyles every line of it. Probe on the Release VM with a
20 000-line block: 6.7 ms per caret step instead of 0.08 ms, entering the block 117 ms instead of
2 ms, both linear in the block size. A 10 MB file in one fence would take about 140 ms per step,
above the brief's 50 ms limit per key event (S007). The benchmark has no large code block.

**Disposition: fixed.** `BlockStyler` keeps the revealed lines and, separately, the opening and
closing fence of the block that holds them. It finds them by walking the cached block states of
the block, without rendering, once per block; a caret step inside the block and typing there
reuse the result as long as the edit adds no fence (an edit on a fence line or one that types a
fence searches again). Only the two fence lines are restyled when the caret enters or leaves. A
walk longer than 4 ms (about 27,000 lines with cached states, fewer right after opening) goes on in
idle slices, and the fences are revealed when found. The benchmark has a 10 MB document that is
one code block (`s021_codeblock_*`, hard budget 50 ms per key): on the development VM caret step
median / max 71.8 / 108.8 ms with b00ea24, 1.8 / 26.4 ms now; typing 72.4 / 102.8 ms, now
1.9 / 8.5 ms (`docs/performance.md`). Tests: `aCaretStepInALargeBlockRendersFewLines`,
`enteringALargeBlockRendersAndRestylesFewLines` (red in 2b4386c), `editsInsideTheBlockKeepTheFencesRevealed`,
`aClosingFenceFarAwayIsRevealedOnceFound`.

## O3 - design - major

`paintCodeBlockPanels` calls `QTextLine::setPosition` on every visible code line while painting
(3 px down). Since S019 `BlockLayout` owns the geometry (ADR 0003): `heightOf`, `hitTest` and
`blockBoundingRect` read the line positions it set. After a paint the lines stand 3 px lower than
the layout assumed when it stored the block heights in the height index; the last line of a block
reaches 3 px into the next block, and the panel bottom is moved down by 3 px over the line below.
Painting must not change layout state.

**Disposition: fixed.** `BlockLayout::setBlockSpacing` gives a block space above its first and
below its last line; the opening fence gets the margin above, the closing fence (or the last line
of a block that runs to the end) below. The space is part of the block height in the height index
and of hit testing; the styler sets it with the formats. `paintCodeBlockPanels` reads the
geometry and moves no line; the panel spans from below the top margin to above the bottom margin.
Tests `theMarginIsPartOfTheLayout`, `paintingMovesNoLine`, `thePanelOverlapsNoNeighbour` (red in
2b4386c). The closing fence now carries a `BlockData` for its margin: one per code block,
`blockDataOnlyOnLinesThatLeaveACodeBlockOpen` expects it.

## O4 - operational - major

The language badge (colour, font, size) and the 3 px offset of the panel are fixed in code. The
theming baseline (brief section 8, ADR 0004, `docs/theming.md`) requires a documented token with a
fallback for every visible property of an element a story adds or changes.

**Disposition: fixed.** New tokens in `[code-block]`: `margin` (space above and below the panel,
0 to 64 px, fallback 3 as in b00ea24), `badge-text` (fallback `code-block.marker`, the colour
b00ea24 used), `badge-font-family` (fallback the editor font) and `badge-font-scale` (0.5 to 2,
fallback 1), documented in `docs/theming.md`. Core tests `S021_CodeBlockFences.ThemeTokensAreParsed`
and `ThemeTokensFallBack`, UI test `theBadgeFontComesFromItsTokens`.

## O5 - operational - minor

The panel used to extend by `code-block.padding` above and below the code; the commit replaces
that with the fixed 3 px. `code-block.padding` is documented as the space between the panel edge
and the code, but now acts horizontally only.

**Disposition: documented.** With the fence lines inside the panel, they are the space between the
panel edge and the code above and below; padding on top of that would make the panel taller by
twice `layout.spacing` (16 px in the built-in themes) than in b00ea24 and Obsidian.
`code-block.padding` is now documented as horizontal (`docs/theming.md`, S021); the space outside
the panel is `code-block.margin`. To be confirmed with S021.

## O6 - premise - minor

The badge shows the whole info string (```` ```python title="x" ```` → `Python title="x"`). In
CommonMark the first word of the info string is the language (spec 4.5; commonmark.js and
markdown-it-py emit `class="language-python"`).

**Disposition: fixed.** The badge shows the first word of the info string with backslash escapes
resolved, its first letter upper-case as in b00ea24: ```` ```python title="x" ```` → `Python`,
```` ```c\+\+ ```` → `C++`, ```` ~~~ a`b c ```` → ``A`b``. Checked with commonmark.js 0.31.2 and
markdown-it-py (commonmark), which agree. Both also decode entity references (`&ouml;l` →
`language-öl`); the badge shows them as written, which S021 records. Tests
`S021_CodeBlockFences.Language*`, `theBadgeShowsTheLanguage` (red in 2b4386c).

## O7 - design - minor

`paintCodeBlockPanels` parses fences itself (leading blanks, fence character, info string). Fence
recognition lives in core (`fenced_code.cpp`, CLAUDE.md: "All logic lives there"); a second parser
can disagree with it, for example on a tilde fence whose info string contains a backtick.

**Disposition: fixed.** The fenced code element reports `LineRender::codeLanguage` for an opening
fence; `EditorWidget::codeBlockBadge` only formats it. The test with a tilde fence whose info
string holds a backtick (`theBadgeShowsTheLanguage`) shows the two parsers had disagreed.

## O8 - operational - minor

The change came in one commit with its tests; the working agreement asks for failing tests first.

**Disposition: resolved.** b00ea24 itself cannot be redone. Its rework on this branch is test-first:
the failing tests for O2–O7 in 2b4386c, then one commit per objection.

## O9 - operational - minor

S009 is extended whenever an element conceals syntax. Fences are now concealed in a new way
(kept space, transparent text, reveal from anywhere in the block); S009 has no scenario for it.

**Disposition: fixed.** S009 has a scenario for code block fences. `tst_s009_concealment` (new)
walks the caret 600 random steps (Up/Down/Left/Right/Home/End/PageUp/PageDown) through a document
with headings, bullets, emphasis, inline code, a backtick and a tilde block and a block that runs
to the end, and checks after every step that the caret is not in concealed text, that both fences
of the caret's block are revealed and that no line of it moved. Residue measurement is still to
come.
