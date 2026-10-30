# S022 — Inline Markdown links and titles

- **Status:** In review · **Order:** 23 · **Depends on:** S013, S015, S020

## Story

As a writer, I want inline links (`[text](url)` and `[text](url "title")`, with angle-bracket URLs, balanced parentheses, and styled text within brackets) to conceal the link syntax markers and display styled link text with hover tooltips and clickable destinations, so that links render cleanly in the live preview while keeping the raw Markdown source intact.

## Acceptance scenarios

**Scenario: standard inline link conceals delimiters and styles link text**
- Given a line with a standard inline link `[Standard Link](https://example.com)`
- When the caret is elsewhere
- Then `[` and `](https://example.com)` are concealed
- And `Standard Link` is visible and styled with the link text colour (`link.text`, falling back to `colors.accent`)
- And the document still contains the original Markdown

**Scenario: active line reveals link markers and destination**
- Given a line with an inline link
- When the caret or selection is on that line
- Then `[` and `](https://example.com)` are visible and styled with `link.marker` (falling back to `colors.marker`)
- And `Standard Link` remains styled with `link.text`

**Scenario: link with title provides tooltip on hover**
- Given `[Link with Title](https://example.com "Hover over me to see the title!")`
- When the line is rendered
- Then the link text has a tooltip set to `"Hover over me to see the title!"`
- And the destination is `https://example.com`

**Scenario: URL with balanced parentheses**
- Given `[Link with Parentheses in URL](https://en.wikipedia.org/wiki/Markdown_(markup_language))`
- When the line is rendered
- Then the destination is parsed as `https://en.wikipedia.org/wiki/Markdown_(markup_language)`
- And parentheses inside the destination do not prematurely terminate the link
- And underscores in the URL are not treated as emphasis delimiters

**Scenario: URL in angle brackets allows spaces**
- Given `[Link with Spaces in URL (Angle Brackets)](<https://example.com/path with spaces/index.html>)`
- When the line is rendered
- Then the destination is parsed as `https://example.com/path with spaces/index.html`
- And the enclosing angle brackets and link delimiters are concealed when not on the active line

**Scenario: nested inline styling inside link brackets**
- Given `[Link with **Bold**, *Italics*, and `Code` Inside Brackets](https://example.com)`
- When the line is rendered
- Then `Bold` receives bold styling, `Italics` receives italic styling, and `` `Code` `` renders as inline code
- And the entire span from `Link with ` to `Inside Brackets` is clickable with destination `https://example.com`
- And formatting markers inside the brackets (`**`, `*`, `` ` ``) are concealed when not active and revealed when active

**Scenario: escaped brackets and non-links stay literal**
- Given `\[not a link](https://example.com)` or `[unclosed link(url)`
- When the line is rendered
- Then it is not parsed as a link and remains regular text

**Scenario: link is themeable (theming baseline, ADR 0004)**
- Given a theme that sets `link.text` and `link.marker`
- When a link is rendered
- Then the link text uses `link.text` and revealed markers use `link.marker`
- And a theme that leaves them unset falls back to `colors.accent` and `colors.marker`

## Tests

- Core: `S022_InlineLinks` verifies delimiter matching, angle-bracket destinations, balanced parentheses, title parsing (double quotes, single quotes, parens), escaped characters, nested inline styling (bold, italic, code), suppression of emphasis in destinations, theme token parsing and fallbacks.
- UI: `tst_s022_inline_links` verifies character formatting, tooltip, anchor href, concealment when inactive, reveal when active, pointing-hand cursor on hover over links, I-beam cursor over text, and clicking links to open them in the browser.
