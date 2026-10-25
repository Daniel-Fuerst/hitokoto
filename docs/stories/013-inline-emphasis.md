# S013 — Inline bold, italic and strikethrough

- **Status:** In Progress · **Order:** 14 · **Depends on:** S003

## Story

As a writer, I want bold, italic and strikethrough text to render inline, so that emphasis is
readable without changing the plain Markdown source.

## Acceptance scenarios

**Scenario: emphasis markers render as styles**
- Given `**important**`, `*emphasis*`, `_emphasis_` and `~~removed~~` on a line
- When the caret is elsewhere
- Then their contents render bold, italic, italic and struck through, respectively
- And the matched delimiter characters are concealed
- And the underlying document still contains the original Markdown

**Scenario: combined and nested emphasis**
- Given nested combinations such as `***bold italic***`, `**bold *italic* bold**` and
  `~~removed **bold removed**~~`
- When the line is rendered
- Then each text range receives the combination of styles indicated by its delimiters
- And only the matched delimiter characters are concealed

**Scenario: source is revealed on the active line**
- Given a line containing matched emphasis markers
- When the caret or a selection is on that line
- Then the raw markers are visible, while the styled text remains formatted

**Scenario: non-matching delimiters stay literal**
- Given unmatched or empty delimiter pairs, whitespace immediately inside a pair, or escaped
  delimiter characters
- When the line is rendered
- Then those characters remain visible and their text receives no emphasis from that pair

**Scenario: underscore does not emphasize inside a word**
- Given `a_b_c`
- When the line is rendered
- Then it remains literal text

## Scope

- Single, double and triple `*`/`_` runs provide italic, bold and combined bold+italic.
- `~~` provides strikethrough.
- Nested and adjacent combinations work within one line. This story does not add other Markdown
  syntax such as links, inline code, or block-level constructs.
- This is a focused inline subset, not a complete CommonMark delimiter-resolution implementation;
  paired delimiter runs must use the same marker and run length.

## Tests

- Core: `S013_InlineEmphasis` verifies parsing into styled ranges, hidden delimiter ranges,
  nesting, unmatched input and escaped delimiters.
- UI: `tst_s013_inline_emphasis` verifies Qt font formats, cursor-aware marker reveal, and that
  rendering never changes the document text.
