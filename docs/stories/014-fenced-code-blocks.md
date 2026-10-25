# S014 — Fenced Markdown code blocks

- **Status:** Done 2026-10-25 · **Order:** 15 · **Depends on:** S013

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

**Scenario: edits update block membership**
- Given text before, within, or at a fence in a code block
- When the text is edited so lines enter or leave the block
- Then affected lines render according to their updated block membership without changing source text

## Scope

- This proposal covers fenced blocks only. An opening fence is a run of at least three backticks
  or tildes, indented by at most three spaces; a backtick fence's info string cannot contain a
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
  updates, and that rendering never changes the document text.

## Implementation

Completed on Linux 2026-10-25. Code content is rendered in a continuous rounded panel painted
behind the text using the theme's surface, spacing and radius tokens; fence lines remain concealed
and outside the panel.
