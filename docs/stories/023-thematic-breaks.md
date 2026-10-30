# S023 — Thematic breaks and setext headings

- **Status:** In progress (implementation on `agents/s023-thematic-break`, 2026-10-30; confirmed by
  Armin on 2026-10-30 after review `docs/reviews/023-thematic-breaks-spec.md`) · **Order:** 24 ·
  **Size:** L ·
  **Depends on:** S002, S003, S014, S018, S019, S021 · **Decision:** ADR 0006
- **Basis:** Armin's feature request from everyday use on Windows (2026-10-28): `---` alone on a line
  shows as text. CommonMark 0.31.2, sections 4.1 (thematic breaks) and 4.3 (setext headings); GFM
  0.29-gfm only where hitokoto already follows GFM (tables, S018). Review O1 (Armin, 2026-10-30):
  setext headings are part of this story, because `---` typed under a line of text is a level-2
  heading in CommonMark and showing it as text would look like the original problem. Review O2
  (Armin): front matter is recognised and left as text; showing it as properties is S024.
- **Changes accepted behaviour of S003**, which kept `---` as plain text while breaks were out of
  scope (`docs/stories/003-bullets.md`, scope).

## Story

As a writer, I want a line of three or more `-`, `*` or `_` to show as a horizontal line, and a line
of text underlined with `===` or `---` to show as a heading, while the caret is elsewhere, and the
characters I typed while the caret is on them, so that a document is divided visually the way it
is when rendered, without the source changing.

## Rules (CommonMark 0.31.2)

**Thematic break (4.1).** Up to three spaces of indentation, then three or more of the **same**
character `-`, `*` or `_`, each optionally followed by spaces or tabs; nothing else on the line.
`---`, `***`, `___`, `- - -`, ` **  * ** * ** * **`, `*\t*\t*`, `   ---   ` are breaks. Not breaks:
`--`, `+++`, `===`, `*-*`, `--- a`, `a---`, `\---`, four spaces or a tab before the characters at the
top level (indented code in CommonMark; hitokoto has none and shows the text). A break needs no
blank line around it, interrupts a paragraph and takes precedence over a list item (`* * *`).

**Setext heading (4.3).** Paragraph text followed by an underline: up to three spaces of
indentation (relative to the content column of the list item the paragraph is in), one or more `=`
(level 1) or `-` (level 2) without blanks between them, then only blanks. Every line of the
paragraph is a heading line of that level; `Foo` / `bar` / `---` is one level-2 heading of two
lines. The underline takes precedence over a break (`Foo` / `---`) and over an empty list item
(`Foo` / `-`). `Foo` / `- - -`, `Foo` / `***` and `Foo` / `___` are breaks.

**Paragraph text** is a line no element claims: not blank, not a heading, break, setext underline,
list item marker line without text, fence or line inside a code block, table row or front matter,
and not the start of a block hitokoto does not render yet (ordered list item, block quote, indented
code, HTML block, link reference definition) where CommonMark lets that block start: after a blank
line or another block any of them; directly after paragraph text only a block quote, an ordered
list item numbered 1 and an HTML block that may interrupt a paragraph, so `Foo` / `2. x` / `---` and
`Foo` / `    bar` / `---` are two-line setext headings.
A bullet with text starts paragraph text at its content column (marker, then 1–4 spaces; 5 or more
count as one, tabs to the next multiple of 4). Directly after paragraph text a bullet without text
cannot start an item (CommonMark 5.2), and no block starts four or more columns past the
paragraph's content column: such lines continue the paragraph as text, so `Foo` / `*` / `---` and
`Foo` / `    - bar` / `---` are two-line setext headings (review O3, code). A line indented less than that column directly after
paragraph text in the item continues it lazily; it cannot be underlined, so `- foo` / `bar` / `---`
ends the list with a break, while `- foo` / `  ---` underlines `foo` inside the item. Such a lazy
line is outside the item, so an ordered list item of any number starts there (`- foo` / `2. y` /
`  ---` is a break). A bullet whose text is a break, a heading or a fence, or that is indented code
(`-      code`), opens no paragraph text.
An ordered list item or a block quote with text is no paragraph text, but the lines directly after
it continue that text lazily, as after a bullet, and can never underline it: `> quote` / `bar` /
`---` and `1. foo` / `bar` / `---` end with a break. Without text, or with a break as its text
(`>`, `1.`, `> ---`), it ends the paragraph: `>` / `bar` / `---` is a setext heading. An HTML block
that may not interrupt a paragraph (a lone tag, CommonMark type 7) needs the whole line:
`<span>x</span>` / `---` is a setext heading.

**Tables (GFM, S018).** A break ends a table: `| a |` / `| - |` / `| 1 |` / `---` is a table and a
break (cmark-gfm). In CommonMark without tables the text would be a setext heading; hitokoto follows
GFM for tables. `| a |` / `---` without a delimiter row is a setext heading in both. The table
element uses the shared rules for where another block starts; a lone HTML tag such as
`<span>x</span>` therefore stays a table row, as in cmark-gfm (S018 ended the table there).

**Front matter (Obsidian, not CommonMark).** `---` on the first line of the document, closed by the
next line `---` (both optionally followed by blanks), is front matter. All its lines, both
delimiters included, are plain text: no break, no heading, no other element (S024 will show it as
properties). Without a closing line, CommonMark applies: the first line is a break.

**Known deviations**, shown as text instead of what CommonMark renders, until the element they
belong to has a story:
- a break inside a list item on the item's marker line (`- * * *`, example 61) stays a bullet with
  text;
- breaks and underlines indented four or more spaces inside a list item (`- foo` / `    ***`): text;
- list membership is tracked only through paragraph text: after a blank line, or after a break,
  heading or indented code inside an item, a paragraph counts as top level, so `- foo`, a blank
  line, `  bar` / `---` and `* foo` / `  * * *` / `  bar` / `-` show a setext heading where
  CommonMark ends the list with a break or starts a new item;
- an empty list item followed by an indented line (`-` / `  bar`) ends the item; in CommonMark the
  line is the item's content;
- an underline inside an ordered list item or a block quote (`1. foo` / `   ---`) shows a break or
  text, because those blocks are not rendered yet;
- a bullet indented four or more columns at the top level (indented code in CommonMark, a nested
  item in hitokoto since S003) opens paragraph text at its content column;
- HTML blocks are not tracked beyond their first line: `<div>` / `---` shows a break, where
  CommonMark keeps `---` as raw HTML of the block;
- link reference definitions: the reference parsers disagree for `[a]: /url` / `---` (commonmark.js
  and markdown-it-py: break, cmark-gfm: text); hitokoto treats the definition as a block start, so
  `---` is a break.

Expected results were checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode `commonmark`)
and, for tables, cmark-gfm 0.29.0.gfm (Python `cmarkgfm` 2025.10.22). During the implementation a
differential run compared `renderDocument` with commonmark.js over 24,000 random documents of the
constructs above (breaks, underlines, ATX headings, bullets, ordered items, quotes, fences, lazy
lines, inline HTML, empty items; without HTML blocks, indented code and link reference definitions,
whose deviations are listed above); after review O3 (code) it found only the deviations listed
here.

## Acceptance scenarios

**Scenario: a break is a horizontal line while the caret is elsewhere**
- Given a line `---`, `***`, `___` or a spaced variant such as `- - -`
- When the caret is on another line
- Then the characters of the line are concealed with their space kept (transparent)
- And a horizontal line is drawn across the text column, centred vertically in the block, in
  `thematic-break.border`, `max(1, round(border-width × device pixel ratio))` device pixels thick and
  aligned to device pixel rows (crisp at 100 % and at 150 % scaling)
- And a break that wraps over several visual lines (narrow window) keeps its height and gets one line
- And the document still contains the original Markdown

**Scenario: the caret on a break reveals its characters**
- Given a thematic break
- When the caret or a selection is on that line
- Then the whole line is shown as written, in `thematic-break.marker`, and no line is drawn
- And the line neither moves nor changes height compared with the concealed state

**Scenario: text underlined with `===` or `---` is a heading**
- Given `Foo` followed by `===` (or `---`), or two lines of text followed by `---`
- When the caret is elsewhere
- Then every text line of the paragraph is shown as a heading of level 1 (or 2), with the heading
  tokens of S002 and the inline styles of S013–S015
- And the underline is concealed with its space kept, so the heading keeps the line below it
- And when the caret or a selection is on any line of the heading or on the underline, the
  underline is shown in `colors.marker`, body size, and no line moves or changes height

**Scenario: editing above or below changes the meaning**
- Given a blank line followed by `---`, shown as a horizontal line
- When I type `Foo` on the blank line
- Then `Foo` becomes a level-2 heading and `---` its underline, revealed while the caret is on `Foo`
- And when I delete the text again, `---` is a horizontal line again
- And when I type `===` under a line of text, that line becomes a level-1 heading

**Scenario: no other element claims a break or an underline**
- Given `* * *`, `***`, `___`, or `-` under text
- Then no bullet, emphasis, escape or heading formatting is produced on that line
- And `---` inside a fenced code block stays code

**Scenario: a break ends a table (GFM)**
- Given a table directly followed by `---`
- Then the table ends above the break and the break is a horizontal line

**Scenario: front matter stays text**
- Given a document that starts with `---`, a line `title: x` and a closing `---`
- Then all three lines are plain text, and the document below them renders as usual
- And without the closing `---`, the first line is a horizontal line

**Scenario: thematic breaks are themeable (theming baseline, ADR 0004)**
- Given a theme with `[thematic-break]`: `border` (colour of the line), `border-width` (0 to 8 px),
  `margin` (space above and below the line, 0 to 64 px), `marker` (characters when revealed)
- Then the line uses these values
- And a theme without them falls back to `colors.border`, 1, 0 and `colors.marker`
- And the margin is part of the block's geometry (ADR 0003), the same concealed and revealed
- And setext headings use the existing heading tokens (`[headings]`) and `colors.marker`, like
  ATX headings (S002); they add no tokens

**Scenario: concealment quality gate (S009)**
- Given breaks, setext headings and front matter among other elements
- When the caret walks with arbitrary sequences of Up/Down/Left/Right/Home/End/PageUp/PageDown
- Then the break the caret is on is revealed and every other break concealed; an underline is
  revealed exactly while the caret is on a line of its heading; nothing in front matter is
  concealed; no line moves or changes height

## Requirements for the implementation

- How lines depend on the lines above and below follows ADR 0006 once accepted. The lazy rendering
  in the UI produces the same `LineRender` for every line as `ElementRegistry::renderDocument`, the
  reference the core tests use. The reference also reveals the fences of the code block the range is
  in (S021), so it covers everything the caret changes.
- One shared thematic-break recogniser in the core, used by the bullet and table elements and by
  S023 (review O10).
- The benchmark gate gets a document with breaks, setext headings and long hard-wrapped and lazily
  continued paragraphs, measuring a cold walk, typing on a break line and typing `- ` before a long
  paragraph (`s023_*`, review O9); all budgets hold.
- S022 (`feature/links_references`, Daniel) touches the same lines: `LineRender::empty()`,
  `theme.hpp` after the escape tokens, `docs/theming.md` and the backlog. Whichever merges second
  keeps both conditions in `empty()` and both token blocks (review O12).
- Known consequences: with `thematic-break.margin` > 0, typing on the blank line above a break turns
  it into an underline and removes the margin, so the text below moves up by twice the margin.
  Front matter reaches from the first line to the next `---` line, however far down and whatever
  lies between, as in Obsidian: a CommonMark document that starts with a break and later has a
  heading underlined with `---` shows everything up to that underline as text (review O5, code;
  test `FrontMatterReachesTheNextDelimiter`).
  ADR 0002 decision 6, if its toggle is switched on, adds a blank line between a table and a row
  without pipes; `foo` / `---` below it then becomes a setext heading.

## Tests

- Core: `S023_ThematicBreaks` – rules above against `ElementRegistry::renderDocument`: recognition,
  setext headings (levels, multi-line paragraphs, list items, precedence over breaks and empty list
  items), paragraph text after unrendered blocks, front matter, code blocks, tables, the render of
  concealed and revealed lines, theme tokens and fallbacks; added with the implementation:
  `LazyTextAfterUnrenderedBlocks` and `InlineHtmlIsParagraphTextAndATableRow` (rules above that the
  first tests did not cover); from the code review `LinesThatCannotInterruptAParagraphContinueIt`,
  `ParagraphRulesOfAnElementComeByRegistrationOnly` and `FrontMatterReachesTheNextDelimiter`.
- UI: `tst_s023_thematic_breaks` – the drawn line (pixels, both ends, crisp, wrapped), revealed
  characters, setext headings (formats, underline revealed from the heading's lines, geometry), the
  margin token, typing above a break, front matter, and the lazy rendering equal to the reference.
  `tst_s023_thematic_breaks_scaled` runs the crispness check at `QT_SCALE_FACTOR=1.5`. From the code
  review: bounded work on the key for an underline under a long paragraph, the underline revealed at
  once in a long heading, and random edits and long paragraphs compared with the reference.
- S009: `tst_s009_concealment::thematicBreaksAndSetextHeadingsWhileWalking`.
- S003: `S003_Bullets.NonItemsArePlainText` no longer expects breaks to be plain text, only that they
  are no items.
