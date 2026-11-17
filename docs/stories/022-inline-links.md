# S022 — Inline Markdown links and titles

- **Status:** Done (accepted by the user on 2026-10-31) · **Order:** 23 · **Depends on:** S013, S015, S020

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
- And the tooltip shows the title as exactly its characters, spaces included, also when the title
  holds tags (`<b>`, `<img …>`): no formatting and no image of the title's own

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

**Scenario: clicking links on concealed and active lines**
- Given a line with a standard inline link `[Standard Link](https://example.com)`
- When the caret is elsewhere (the line is concealed)
- Then clicking on `Standard Link` opens `https://example.com` in the default browser (an exception to S010 for clickable link navigation)
- When the caret is already on that line (the line is active and markers are revealed)
- Then a plain click on `Standard Link` places the caret at the clicked position for editing (per S010) and does not open the link
- And a Ctrl+click (or Cmd+click on macOS) on `Standard Link` on the active line opens `https://example.com` in the default browser

**Scenario: relative destinations and anchor fragments stay non-clickable until document resolution is specified**
- Given a line with `[Story](000-repository-skeleton.md)` or `[Anchor](#intro)`
- When clicked on either active or concealed lines
- Then the link is not dispatched to the system shell or browser (safe default avoiding process-relative path execution)
- And the click places the caret for editing per S010 until document-relative link resolution is specified in a future story

**Scenario: link is themeable (theming baseline, ADR 0004)**
- Given a theme that sets `link.text` and `link.marker`
- When a link is rendered
- Then the link text uses `link.text` and revealed markers use `link.marker`
- And a theme that leaves them unset falls back to `colors.accent` and `colors.marker`

## Tests

- Core: `S022_InlineLinks` verifies delimiter matching, angle-bracket destinations, balanced parentheses, title parsing (double quotes, single quotes, parens), destinations starting with quotes/parens, escaped characters, nested inline styling (bold, italic, code), badge/interleaved images inside links, suppression of emphasis in destinations, theme token parsing and fallbacks. Also covers CommonMark negatives (emphasis across link boundaries, links inside links, image syntax remaining literal), unclosed parenthesis links, avoidance of iterator invalidation, and linear bracket-matching performance.
- UI: `tst_s022_inline_links` verifies character formatting, tooltip text, anchor href, concealment when inactive, reveal when active, pointing-hand cursor on hover over browser links, I-beam cursor over text, clicking links on concealed lines to open in the browser, clicking on active lines to place caret while Ctrl+clicking opens the link, clicking badge links containing images to open the outer destination, and rejection of unsafe schemes or relative URLs.
- Quality Gate: `tst_s009_concealment` extends the standing concealment quality gate with inline links.
- Benchmarks: `S022_RenderLineWithLinks` and `S022_RenderLineWithUnclosedBrackets` in `bench/core_bench.cpp`.

## Rework on 2026-11-15

On `agents/s018-table-cell-content`: Qt shows a tooltip as rich text when its text looks like rich
text. The format of a link's text now carries the title as rich text that shows exactly its
characters and looks as plain text did (`Qt::convertFromPlainText` with `Qt::WhiteSpacePre`), so
every way Qt shows the tooltip of the format shows the title as its characters; the tooltip of an
unsupported front matter value (S024) gets the same treatment. Tests:
`tst_s022_inline_links` `aTitleWithTagsShowsAsItsCharacters` (a title with tags and two spaces, the
tooltip shows its characters and no image) and the title checks of the other tests compare the
text the tooltip shows; `tst_s024_front_matter` `theTooltipOfAValueShowsItsCharacters`.

## Rework on 2026-11-16

Branch `agents/s013-inline-in-linear-time`, from main 0b69199.

- A line above the bounds of S007 (longer than 10,000 characters, or with more than 300 characters
  that can start inline syntax) shows as text: no links are rendered on it. See
  `007-large-files.md`, "Rework on 2026-11-16".
- A line renders in linear time. Before, every `](` that did not become a link read on to the end
  of the line: a destination whose parentheses never balance (`[](` and `[a](` repeated: 19 s and
  11 s for two renders of 200,000 characters), a title in parentheses that never ends
  (`[a](b (`). The destination and the title were also copied out of the line before it was clear
  that there is a link.
- Where a destination or a title ends is now answered for all positions of the line at once
  (`LinkScans` in `link.cpp`): the answer for a position follows from the answers for the
  positions to its right, so one pass from the end of the line fills a table, built when the first
  `](` asks. The texts are taken out of the line only once the link stands. A single backtick
  that no later backtick closes is looked for once per line, not once per backtick.
- The results are unchanged, also where they differ from CommonMark (a title in parentheses that
  holds `(`, a backslash before a blank in a destination): the random lines named in
  `013-inline-emphasis.md` give the same links, destinations and titles as before.
- Tests: `S022_InlineLinks` `ALineRendersInLinearTime` (steps of the inline pass for 25 patterns,
  `tests/core/line_work.hpp`), `RandomLinesOfSyntaxRenderInLinearTime` (120 lines put together
  from pieces of syntax with a fixed seed: at most 100 steps per character),
  `ParenthesesOfADestinationBalanceOrThereIsNoLink`, `ATitleEndsAtItsDelimiterOrThereIsNoLink` and
  `EmphasisInsideALinkAndAroundALink` (each line checked against commonmark.js 0.31.2 and
  markdown-it-py 4.2.0). `LinearBracketMatchingPerformance` stays; its line is above the bounds
  of S007 now and shows as text.
