# S013 — Inline bold, italic and strikethrough

- **Status:** Done (accepted by the user on 2026-10-27) · **Order:** 14 · **Depends on:** S003

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
- Given nested combinations such as `***bold italic***`, `**bold *italic* bold**`,
  `**bold *boldanditalic***` and `~~removed **bold removed**~~`
- When the line is rendered
- Then each text range receives the combination of styles indicated by its delimiters
- And only the matched delimiter characters are concealed
- And a combined closing run can close nested emphasis of the same marker

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

**Scenario: emphasis is themeable (theming baseline, ADR 0004)**
- Given a theme that sets `emphasis.bold-weight`, `emphasis.bold-text`, `emphasis.italic-text`,
  `emphasis.strikethrough-text` and `emphasis.marker`
- When emphasis is rendered, with the caret elsewhere and on the line
- Then each part uses its token: bold text the weight and colour, italic and struck-through text
  their colours, revealed delimiters the marker colour
- And a theme that sets none of them renders emphasis in the surrounding text colour, bold at
  weight 700, revealed delimiters in `colors.marker`

## Scope

- Single, double and triple `*`/`_` runs provide italic, bold and combined bold+italic; a triple
  run can also resolve as nested italic and bold delimiters, including across separate closing runs.
- `~~` provides strikethrough, which can be nested with `*`/`_` emphasis.
- Nested and adjacent combinations work within one line. This story does not add other Markdown
  syntax such as links, inline code, or block-level constructs.
- This is a focused inline subset, not a complete CommonMark delimiter-resolution implementation;
  delimiters that form a pair must use the same marker, and `~` remains a two-character delimiter.
- Bold inside a heading never renders lighter than the heading. With the built-in themes it looks
  like the heading (both weights are 700), as in Obsidian with its default settings; making it
  visible by default would change every heading and is left to a later theme decision (review O6,
  2026-10-26). `docs/theming.md` says how a theme makes it visible.

## Tests

- Core: `S013_InlineEmphasis` verifies parsing into styled ranges, hidden delimiter ranges,
  nested and shared delimiter runs, unmatched input and escaped delimiters.
- UI: `tst_s013_inline_emphasis` verifies Qt font formats, cursor-aware marker reveal, and that
  rendering never changes the document text.
- Theming: core tests `S013_InlineEmphasis` parse every `emphasis.*` token and check its fallback;
  UI tests check that each token changes the rendered format.

## Implementation

Implemented 2026-10-25. Core and UI tests pass on Linux. Bold weight, the three text colours and
the colour of revealed delimiters come from `[emphasis]` (ADR 0004).

CI run #18 (pull request #5, 2026-10-25) is green on Linux (Qt 6.12 and 6.8), Windows (Qt 6.8.3)
and macOS (Qt 6.12).
