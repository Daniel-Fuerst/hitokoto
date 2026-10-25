# ADR 0004: Element theme tokens with fallbacks

- **Status:** Accepted
- **Date:** 2026-10-25
- **Deciders:** project owner

## Context

The brief (section 8) requires every styleable element to be themeable on its own, with a token
for every property it visibly has, and makes this a baseline for every story. Until now elements
reused general tokens: code blocks and inline code take their background from `colors.surface`,
their radius from `layout.radius` and their padding from `layout.spacing`; emphasis has no tokens.

The theme file format is user-facing. Once people write themes, renaming or restructuring tokens
breaks them, so the structure is decided once, here.

## Decision

1. **One section per element**, named after the element: `[code-block]`, `[inline-code]`,
   `[emphasis]`, and later `[link]`, `[blockquote]`, … The existing sections (`[colors]`,
   `[fonts]`, `[headings]`, `[layout]`, `[buttons]`) stay as they are.
2. **Shared property names.** A property that several elements have uses the same key
   everywhere: `background`, `text`, `border`, `border-width`, `radius`, `padding`,
   `font-family`, `font-scale`, `font-weight`, `marker` (colour of the element's revealed syntax
   characters). An element gets only the properties it visibly has; compound names
   (`bold-weight`, `italic-text`) are used where one element has several parts.
3. **Every element token has a documented fallback**, either a general token or a fixed value.
   A token that is not set in the theme file (nor in its base theme) takes the fallback.
4. **Fallbacks are resolved when the value is used, not when the file is parsed.** A theme with
   `base = "dark"` that only changes `colors.surface` therefore also changes the code block
   background, as long as neither theme sets `code-block.background`. The built-in themes leave
   element tokens unset unless no general token fits.
5. **Sizes of text elements are relative.** `font-scale` multiplies the size of the surrounding
   text, so inline code in a heading stays heading-sized.
6. **Adding an element adds its section.** Its tokens, types, ranges and fallbacks are listed in
   `docs/theming.md`; the existing test that checks the documentation covers them.

### Tokens of the elements in S013–S015

| Token | Type | Fallback |
|---|---|---|
| `code-block.background` | color | `colors.surface` |
| `code-block.text` | color | `colors.text` |
| `code-block.border` | color | `colors.border` |
| `code-block.border-width` | number (px), 0 to 8 | 0 |
| `code-block.radius` | number (px), 0 to 32 | `layout.radius` |
| `code-block.padding` | number (px), 0 to 64 | `layout.spacing` |
| `code-block.font-family` | string | "" = system fixed-width font |
| `code-block.font-scale` | number, 0.5 to 2 | 1 |
| `code-block.marker` | color | `colors.marker` |
| `inline-code.background` | color | `code-block.background` |
| `inline-code.text` | color | `code-block.text` |
| `inline-code.border` | color | `code-block.border` |
| `inline-code.border-width` | number (px), 0 to 8 | 0 |
| `inline-code.radius` | number (px), 0 to 32 | `code-block.radius` |
| `inline-code.padding` | number (px), 0 to 16 | 2 |
| `inline-code.font-family` | string | `code-block.font-family` |
| `inline-code.font-scale` | number, 0.5 to 2 | 1 |
| `inline-code.marker` | color | `colors.marker` |
| `emphasis.bold-weight` | number, 100 to 900 | 700 |
| `emphasis.bold-text` | color | text colour of the surrounding element |
| `emphasis.italic-text` | color | text colour of the surrounding element |
| `emphasis.strikethrough-text` | color | text colour of the surrounding element |
| `emphasis.marker` | color | `colors.marker` |

`fonts.code-family` (added on this branch, never released) is replaced by
`code-block.font-family` and `inline-code.font-family`.

## Consequences

- Short themes stay short; a full theme can still set every element on its own.
- The `Theme` struct keeps element tokens as optional values plus one resolving accessor per
  token; UI code reads only the accessors, so a fallback is defined in exactly one place.
- The built-in themes look as before, with three visible changes: revealed fence lines use the
  marker colour instead of the text colour, and inline code gets rounded corners
  (`code-block.radius`) and 2 px horizontal padding.
- Inline code radius, padding and border need the editor to paint the inline background itself,
  as it already does for the code block panel; a character format cannot round corners or pad.
  Horizontal padding moves the following text, like padding in CSS.
- The general tokens and the elements of S002, S003 and S005 (headings, bullets, markers, UI
  widgets) are unchanged; giving them their own sections would be separate stories.
