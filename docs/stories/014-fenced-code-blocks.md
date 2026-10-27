# S014 — Fenced Markdown code blocks

- **Status:** In review · **Order:** 15 · **Depends on:** S013

## Story

As a writer, I want fenced code blocks to render as code, so that examples remain readable
without changing the plain Markdown source.

## Acceptance scenarios

**Scenario: fenced blocks render as code**
- Given a fenced block delimited by at least three matching backticks or tildes
- When the caret is outside the block
- Then its content is displayed in a monospace code style inside one continuous, padded,
  theme-driven block panel spanning the code lines
- And opening and closing fence markers are concealed
- And the concealed fence lines do not appear as panel content
- And the document still contains the original Markdown

**Scenario: source is revealed on a fence line**
- Given a fenced block with concealed markers
- When the caret or selection is on an opening or closing fence line
- Then that line's raw fence and any info string are visible
- And the block content remains styled as code

**Scenario: Markdown syntax inside code stays literal**
- Given code containing headings, bullets, emphasis delimiters, or other Markdown-like text
- When the block is rendered
- Then its content is not rendered as those Markdown elements

**Scenario: fence variants and unterminated blocks**
- Given a block opened with backticks or tildes, optionally followed by an info string
- When it is closed with the same fence character and a run at least as long as the opener
- Then the block ends at that closing line
- And a block without a closing fence continues through the end of the document

**Scenario: code blocks are themeable (theming baseline, ADR 0004)**
- Given a theme that sets every `code-block.*` token (background, text, border, border-width,
  radius, padding, font-family, font-scale, marker)
- When a code block is rendered, with the caret outside and on a fence line
- Then the panel uses the background, border, border width, radius and padding; the code uses
  the text colour, font family and scale; revealed fence lines use the marker colour
- And a theme that sets none of them falls back as documented: `colors.surface`, `colors.text`,
  `colors.border`, no border, `layout.radius`, `layout.spacing`, the system fixed-width font,
  scale 1, `colors.marker`
- And changing a general token such as `colors.surface` in a user theme changes the code block
  as long as the theme does not set the element token

**Scenario: edits update block membership**
- Given text before, within, or at a fence in a code block
- When the text is edited so lines enter or leave the block
- Then affected lines render according to their updated block membership without changing source text

## Scope

- This proposal covers fenced blocks only. An opening fence is a run of at least three backticks
  or tildes, optionally preceded by whitespace (spaces or tabs); a backtick fence's info string cannot contain a
  backtick. A closing fence uses the same character, has a run at least as long as the opener,
  and may have only whitespace after it.
- The info string is preserved and revealed with the opening fence; language-specific syntax
  highlighting is not included.
- Code content is displayed literally. Indented code blocks, CommonMark delimiter edge cases
  beyond those above, and syntax highlighting are out of scope.
- Rendering and block membership must be incremental; editing a fence must update the affected
  lines without requiring a full-document parse on every keystroke.

## Tests

- Core: `S014_FencedCodeBlocks` verifies fence recognition, block membership, concealed ranges,
  literal code content, unterminated blocks, and updates when fence text changes.
- UI: `tst_s014_fenced_code_blocks` verifies code styling, cursor-aware fence reveal, incremental
  updates, that rendering never changes the document text, and that only lines leaving a code
  block open carry per-block metadata (memory footprint).
- Theming: core tests `S014_FencedCodeBlocks` parse every `code-block.*` token and check its
  fallback, including a general token changed in a user theme; UI tests check formats and the
  painted panel (colour, radius, border, padding) by pixel sampling.

## Implementation

Completed on Linux 2026-10-25. Code content is rendered in a continuous rounded panel painted
behind the text using the theme's surface, spacing and radius tokens; fence lines remain concealed
and outside the panel. Panel and code take every `code-block.*` token (ADR 0004); revealed fence lines use
`code-block.marker`.

CI run #18 (pull request #5, 2026-10-25) is green on Linux (Qt 6.12 and 6.8), Windows (Qt 6.8.3)
and macOS (Qt 6.12).
