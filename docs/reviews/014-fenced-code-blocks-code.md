---
mode: code
spec: docs/stories/014-fenced-code-blocks.md
branch: fix/block-comments-backticks-and-margin  # Daniel's branch, one commit
commit_range: e997683..b00ea24
reviewed_on: 2026-10-28
dispositioned: open (branch agents/s014-fences-in-block)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), presets dev and ci; merge with main 759a59d
objections:
  - id: O1
    category: premise
    severity: major
    title: S014 (Done) rewritten in place instead of a new story
    disposition: open
  - id: O2
    category: failure
    severity: major
    title: Every caret step walks the whole code block; cost grows linearly with its size
    disposition: open
  - id: O3
    category: design
    severity: major
    title: paintCodeBlockPanels moves QTextLines while painting; BlockLayout owns the geometry
    disposition: open
  - id: O4
    category: operational
    severity: major
    title: Language badge and the 3 px panel offset have no theme tokens
    disposition: open
  - id: O5
    category: operational
    severity: minor
    title: The vertical part of code-block.padding no longer applies
    disposition: open
  - id: O6
    category: premise
    severity: minor
    title: The badge shows the whole info string instead of the language
    disposition: open
  - id: O7
    category: design
    severity: minor
    title: A second fence parser in the UI
    disposition: open
  - id: O8
    category: operational
    severity: minor
    title: Not test-first
    disposition: open
  - id: O9
    category: operational
    severity: minor
    title: S009 not extended for the new concealment
    disposition: open
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

**Disposition: open.**

## O2 - failure - major

`BlockStyler::setRevealed` calls `expandCodeBlocks`, which walks from the caret line to both
fences and renders every line on the way, on every caret move. The revealed range then spans the
whole block, so entering or leaving it restyles every line of it. Probe on the Release VM with a
20 000-line block: 6.7 ms per caret step instead of 0.08 ms, entering the block 117 ms instead of
2 ms, both linear in the block size. A 10 MB file in one fence would take about 140 ms per step,
above the brief's 50 ms limit per key event (S007). The benchmark has no large code block.

**Disposition: open.**

## O3 - design - major

`paintCodeBlockPanels` calls `QTextLine::setPosition` on every visible code line while painting
(3 px down). Since S019 `BlockLayout` owns the geometry (ADR 0003): `heightOf`, `hitTest` and
`blockBoundingRect` read the line positions it set. After a paint the lines stand 3 px lower than
the layout assumed when it stored the block heights in the height index; the last line of a block
reaches 3 px into the next block, and the panel bottom is moved down by 3 px over the line below.
Painting must not change layout state.

**Disposition: open.**

## O4 - operational - major

The language badge (colour, font, size) and the 3 px offset of the panel are fixed in code. The
theming baseline (brief section 8, ADR 0004, `docs/theming.md`) requires a documented token with a
fallback for every visible property of an element a story adds or changes.

**Disposition: open.**

## O5 - operational - minor

The panel used to extend by `code-block.padding` above and below the code; the commit replaces
that with the fixed 3 px. `code-block.padding` is documented as the space between the panel edge
and the code, but now acts horizontally only.

**Disposition: open.**

## O6 - premise - minor

The badge shows the whole info string (```` ```python title="x" ```` → `Python title="x"`). In
CommonMark the first word of the info string is the language (spec 4.5; commonmark.js and
markdown-it-py emit `class="language-python"`).

**Disposition: open.**

## O7 - design - minor

`paintCodeBlockPanels` parses fences itself (leading blanks, fence character, info string). Fence
recognition lives in core (`fenced_code.cpp`, CLAUDE.md: "All logic lives there"); a second parser
can disagree with it, for example on a tilde fence whose info string contains a backtick.

**Disposition: open.**

## O8 - operational - minor

The change came in one commit with its tests; the working agreement asks for failing tests first.

**Disposition: open.**

## O9 - operational - minor

S009 is extended whenever an element conceals syntax. Fences are now concealed in a new way
(kept space, transparent text, reveal from anywhere in the block); S009 has no scenario for it.

**Disposition: open.**
