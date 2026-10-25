# S015 — Inline Markdown code spans

- **Status:** In progress (CI pending) · **Order:** 16 · **Depends on:** S014

## Story

As a writer, I want single-backtick code spans to render as inline code, so that short
code-like text is visually distinct without changing the Markdown source.

## Acceptance scenarios

**Scenario: inline code renders with concealed delimiters**
- Given a non-empty span surrounded by matching single backticks
- When the caret is elsewhere on the line
- Then the contents use a fixed-width font and the theme's code surface and text colors
- And the delimiters are concealed
- And the document still contains the original Markdown

**Scenario: active line reveals inline-code delimiters**
- Given a line containing an inline code span
- When the caret or selection is on that line
- Then the backticks are visible and the contents remain styled as inline code

**Scenario: Markdown syntax inside inline code stays literal**
- Given text such as `**literal**`, `_literal_`, or `# literal` inside a code span
- When the line is rendered
- Then the text receives no emphasis or block Markdown styling from its contents

**Scenario: non-matching and escaped backticks stay literal**
- Given an unmatched or escaped backtick
- When the line is rendered
- Then it remains visible and does not start an inline code span

## Scope

- This story adds spans delimited by one backtick on a single line. Multiline spans and
  multi-backtick delimiters are out of scope.
- Code text is styled but not syntax highlighted. Fenced code blocks are handled by S014.
- Rendering never modifies the source text.

## Tests

- Core: `S015_InlineCode` verifies delimiter matching, hidden ranges, literal Markdown-like
  contents, and active-line reveal.
- UI: `tst_s015_inline_code` verifies the inline fixed-width theme formatting and confirms that
  the document retains the original Markdown.

## Implementation

Completed on Linux 2026-10-25. Single-backtick spans use the inline-code role and are excluded
from emphasis parsing; the source remains unchanged. The font is `fonts.code-family`, as for code blocks.
