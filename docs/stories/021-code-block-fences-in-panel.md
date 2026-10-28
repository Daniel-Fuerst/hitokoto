# S021 — Code block fences inside the panel

- **Status:** Proposed (to be confirmed with Daniel; implemented on `agents/s014-fences-in-block`) ·
  **Order:** 22 · **Depends on:** S014, S019
- **Basis:** Daniel's commit b00ea24 on `fix/block-comments-backticks-and-margin` (2026-10-27),
  which changed S014's accepted behaviour to match Obsidian. Scope changes become new stories
  (review `docs/reviews/014-fenced-code-blocks-code.md`, O1); this story records the change and its
  reason. The rework of the review objections lives on `agents/s014-fences-in-block`.

## Reason

With S014 a concealed fence line collapsed, so entering or leaving a code block made it jump: the
fence lines appeared, the block grew by two lines, and the text below moved. The fence lines also
stood half outside the panel. Obsidian keeps the block still: the fence lines are part of the
block, hidden while the caret is elsewhere, shown while it is anywhere in the block, and the
opening line shows the language.

## Story

As a writer, I want a code block to keep its size and place while I move the caret into it and
out of it, and to see its language at a glance, so that moving through a document with code does
not make the text jump.

## Acceptance scenarios

**Scenario: concealed fences keep their space inside the panel**
- Given a fenced code block and the caret outside it
- Then the panel spans from the opening fence line to the closing fence line
- And the fence characters and the info string are concealed, each fence line keeping its height
- And there is a margin above and below the panel, so it overlaps neither the line above nor the
  line below

**Scenario: the caret anywhere in the block reveals both fences**
- Given a fenced code block
- When the caret or a selection is on any line of the block (a fence line or a code line)
- Then both fence lines show their source, including the info string, in the marker colour
- And no line of the block moves or changes height compared with the concealed state
- And a selection that starts in one block and ends in another reveals the opening fence of the
  first and the closing fence of the last

**Scenario: the language badge**
- Given an opening fence with an info string and the caret outside the block
- Then the first word of the info string (the language in CommonMark, with backslash escapes
  resolved) is shown right-aligned on the opening fence line, its first letter upper-case
- And an opening fence without an info string shows no badge
- And entity references in the info string are shown as written (not decoded)

**Scenario: large blocks stay fast**
- Given a 10 MB document that is one code block
- When the caret moves inside the block, enters it or leaves it, and when I type in it
- Then no key event takes longer than 50 ms (brief, section 4), and a caret step inside the block
  does not depend on the size of the block

**Scenario: themeable (theming baseline, ADR 0004)**
- Given a theme that sets `code-block.margin`, `code-block.badge-text`,
  `code-block.badge-font-family` and `code-block.badge-font-scale`
- Then the panel keeps that margin above and below, and the badge uses that colour, font family
  and size
- And a theme that sets none of them falls back as documented: 3 px, `code-block.marker`, the
  editor font, scale 1

## Scope

- Changes S014's scenarios "fenced blocks render as code" (fence lines are panel content now) and
  "source is revealed on a fence line" (the whole block reveals both fences). Everything else in
  S014 stays.
- Vertically, the fence lines are the space between the panel edge and the code;
  `code-block.padding` is horizontal (`docs/theming.md`).
- The margin is part of the block's height in the document layout (ADR 0003), never applied while
  painting.

## Tests

- Core: `S021_CodeBlockFences` – the language of an opening fence (first word, escapes), the new
  tokens and their fallbacks.
- UI: `tst_s021_code_block_fences` – reveal from anywhere in the block without geometry change,
  margin in the layout, painting changes no line position, panel overlaps no neighbour, badge text
  and font, few lines rendered per caret step in a 20 000-line block.
- S009: `tst_s009_concealment` – the caret never stands in concealed text while walking through
  code blocks.
- Benchmark: `s021_codeblock_*` on a 10 MB document that is one code block.

## Implementation

Daniel's commit b00ea24, reworked on `agents/s014-fences-in-block` after the review
(`docs/reviews/014-fenced-code-blocks-code.md`), 2026-10-28, Linux only so far (development VM,
Qt 6.12.0, presets dev and ci):

- `BlockStyler` reveals the caret's lines plus the two fences of the code block that holds them.
  It finds the fences by walking cached block states (no rendering) once per block and keeps the
  result while the caret moves inside the block and while typing there adds no fence. A walk
  that takes more than 4 ms (a large block, or one whose states are not known yet after opening)
  goes on in idle slices; the fences are revealed when found.
- The margin is space above the opening and below the closing fence line in `BlockLayout`, part
  of the block heights in the height index; painting moves no line.
- Core reports the language of an opening fence (`LineRender::codeLanguage`); the editor has no
  fence parser of its own.

Benchmark on a 10 MB document that is one code block (`s021_codeblock_*`, development VM, release
build, offscreen), Daniel's commit against the rework: caret step median / p95 / max 71.8 / 87.1 /
108.8 ms → 1.8 / 7.4 / 26.4 ms; entering the block max 134.6 → 8.6 ms; typing median / p95 / max
72.4 / 83.5 / 102.8 → 1.9 / 2.5 / 8.5 ms (the slower of two runs, `docs/performance.md`).
