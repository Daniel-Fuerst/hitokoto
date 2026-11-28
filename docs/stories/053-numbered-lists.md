# S053 — Numbered lists

- **Status:** In progress (confirmed by the user on 2026-11-28) · **Order:** 58 · **Size:** M ·
  **Depends on:** S003, S009, S023, S033, S050
- **Basis:** In the brief's list of later features (section 1, "numbered lists"); entry "Numbered
  lists" in [`unrefined.md`](unrefined.md), which this story refines. Decisions of the user on
  2026-11-28: the number is shown as written, in its own colour; Enter writes the next number and
  renumbers the items below; nesting by content column stays out of this story (entry "Indentation
  of bullet lists" in `unrefined.md`).
- **Changes accepted behaviour of S023** ("Known deviations": an underline inside an ordered list
  item, `1. foo` / `   ---`, showed a break or text because ordered items were not rendered). An
  ordered item now opens paragraph text at its content column like a bullet (ADR 0006), so
  `1. foo` / `   ---` is a level-2 setext heading inside the item, as in CommonMark.

## Story

As a writer, I want lines like `1. milk` to read as a numbered list, and Enter to keep the numbers
in order, so that I can write steps and rankings without counting by hand, while the file stays
plain Markdown.

## Decisions

1. **What an ordered item is (CommonMark 0.31.2, 5.2).** Optional indentation, 1 to 9 digits, `.`
   or `)`, then a blank or the end of the line. `1.x`, `1234567890. x`, `a. x` and `1 . x` are no
   items. As for bullets (S003), the indentation is the nesting depth (two columns a level); nesting
   by content column is a later story.
2. **The number is shown as written.** The text is the only truth (ADR 0002): `3.` stays `3.`, also
   while the caret is elsewhere; hitokoto never shows a number the file does not hold. Concealed, the
   number and its delimiter are drawn in the `ordered-list` tokens (colour, weight, font family);
   revealed (caret or selection on the line), in `ordered-list.marker` like other revealed syntax.
   Nothing is hidden, so the caret and clicks behave as on plain text (S009, S010 need no change).
3. **Paragraph rules (CommonMark 5.2, ADR 0006).** An item with text opens paragraph text at its
   content column (after the delimiter and one to four columns of padding), so continuation lines
   flow (S033) and a setext underline at the content column underlines the item's text (S023).
   Directly after paragraph text an item starts a list only if it is numbered 1 and has text, or the
   line is lazy (less indented than the paragraph's content); otherwise it continues the paragraph
   (`Foo` / `2. x` is one paragraph). Like S050 for bullets, a marker followed by a blank and nothing
   else (`2. `, what Enter writes) is an empty item even directly after text.
4. **Enter continues the list and renumbers it.** Enter at or after the prefix of `2. milk` writes a
   new line `3. ` with the same indentation, delimiter and padding. Enter on an empty item outdents a
   nested item by its marker width, or removes the marker of a top-level item (as S003). After every
   such Enter the list the caret is in is renumbered: its first item keeps its number (the start,
   CommonMark: `<ol start>`), each following item gets the number before it plus one. A list is the
   run of items at the same indentation with the same delimiter, with the lines between them that
   are blank or indented further (nested items, continuation lines); a change of delimiter, a
   bullet at that indentation, or a less indented non-blank line ends it. Only the digits of items
   whose number changes are rewritten; the edit and the renumbering are one undo step.
5. **Tab and Shift+Tab** indent and outdent an ordered item by its marker width (digits, delimiter
   and one space, so `1. a` / `2. b` nests `b` under `a`), from anywhere on the line, and renumber:
   an item that becomes the first of a nested list gets the number 1; the list it left and the list
   it joined are renumbered as in decision 4.
6. **Theming baseline (brief, section 8):** a new section `[ordered-list]` with `text` (colour of
   the concealed number, default `colors.bullet`, as the bullet dot), `marker` (colour when revealed,
   default `colors.marker`), `weight` (font weight, default 400) and `font-family` (default the
   editor font). Documented in `docs/theming.md`.
7. **Extension by addition (brief, section 5):** a new line element in its own file, registered with
   one line in `ElementRegistry::withBuiltins`; no file of another element changes.

## Acceptance scenarios

**Scenario: number shown as written**
- Given the line `3. milk` and the caret elsewhere
- Then `3.` is drawn in `ordered-list.text` and weight `ordered-list.weight`, `milk` after it, and
  nothing of the line is hidden

**Scenario: raw marker on the cursor line**
- When the caret is on that line
- Then `3.` is drawn in `ordered-list.marker`

**Scenario: what is an item**
- Given `1. a`, `1) a`, `10. a`, `123456789. a`, `1.`, `1.\ta`, `  2. a`
- Then each is an ordered item; `1.a`, `1234567890. a`, `a. x`, `1 . x` and `-1. x` are not

**Scenario: wrapped lines have a hanging indent**
- Given an item whose text wraps across several visual lines
- Then the continuation lines start under the item's text, concealed or revealed

**Scenario: interrupting a paragraph**
- Given `Foo` / `1. bar`, the second line is an item; given `Foo` / `2. bar` or `Foo` / `1.`, the
  second line continues the paragraph `Foo`; given `Foo` / `2. `, the second line is an empty item

**Scenario: underline inside an item**
- Given `1. foo` / `   ---`, `foo` is a level-2 heading; given `1. foo` / `---`, the second line is
  a thematic break

**Scenario: Enter continues and renumbers**
- Given `1. a` / `2. b` / `3. c` and the caret at the end of `1. a`
- When I press Enter
- Then the lines are `1. a` / `2. ` / `3. b` / `4. c`, the caret after `2. `, and one undo restores
  the three lines

**Scenario: start number and delimiter are kept**
- Given `7) a` and the caret at its end
- When I press Enter
- Then the next line is `8) `

**Scenario: Enter on an empty item**
- Given `1. a` / `2. ` / `3. b` and the caret on `2. `
- When I press Enter
- Then the marker is removed, leaving a blank line, and the list is renumbered: `1. a` / `` /
  `2. b` (a blank line between items keeps them in one list, CommonMark 5.3)

**Scenario: Tab nests and renumbers**
- Given `1. a` / `2. b` / `3. c` and the caret on `2. b`
- When I press Tab
- Then the lines are `1. a` / `   1. b` / `2. c`; Shift+Tab gives back `1. a` / `2. b` / `3. c`

**Scenario: renumbering stays inside its list**
- Given `1. a` / `   1. x` / `   1. y` / `2. b` / `- c` / `1. d` and Enter at the end of `1. a`
- Then the nested items and `1. d` after the bullet are not changed; `2. b` becomes `3. b`

## Tests

- **Core (`tests/core/test_s053_numbered_lists.cpp`):** `S053_NumberedLists` — parse rule (what is
  an item), render mapping concealed and revealed, `wrapIndent`, paragraph rules and setext
  underlines, `parseOrderedLine`, `renumberOrderedList` (start kept, nested lists and other lists
  untouched, only changed digits rewritten, delimiter change ends a list), indentation helpers,
  theme tokens with their fallbacks.
- **Core:** `S023_ThematicBreaks` tests that treated ordered items as unrendered blocks now expect
  the item rules of decision 3.
- **UI (`tests/ui/tst_s053_numbered_lists.cpp`):** number colour concealed and revealed, Enter
  continuation and renumbering in one undo step, Enter on an empty item, Tab and Shift+Tab.
