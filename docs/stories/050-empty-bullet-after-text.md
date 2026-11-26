# S050 — An empty bullet under text stays a bullet

- **Status:** In progress (confirmed by the user on 2026-11-25) · **Order:** 54 · **Size:** S ·
  **Depends on:** S003, S023
- **Basis:** Bug report by the user on 2026-11-25. Typing `- first bullet`, Enter, Tab,
  `second bullet`, Enter, Tab gives

  ```markdown
  - first bullet
    - second bullet
      -
  ```

  and "second bullet" is shown as a level-2 heading. CommonMark 0.31.2 reads it that way: an empty
  list item may not interrupt a paragraph (5.2), so `    - ` lies at the content column of "second
  bullet" and is a setext underline (4.3). S023 follows that (`Foo` / `-` is a heading).
- **Changes accepted behaviour of S023** (`docs/stories/023-thematic-breaks.md`, "Setext heading" and
  "Paragraph text") and of S003 (notes). Decision of the user on 2026-11-25: a marker followed by a
  blank is an empty bullet; a bare marker keeps CommonMark.

## Story

As a writer building a nested list, I want a new, still empty bullet to show as a bullet, so that the
item above does not turn into a heading while I type.

## Decisions

1. **A marker followed by a blank starts an empty item, even directly after paragraph text.** `- `,
   `* ` and `+ ` (one or more spaces or tabs, nothing else) are an empty list item where CommonMark
   would continue the paragraph above as text. This is what Enter (auto-continuation) and Tab write.
   The rule covers all three markers, so that `*` and `+` lists behave like `-` lists while typing.
2. **`- ` is no setext underline.** A single `-` followed by a blank underlines nothing. `-`
   without the blank, `--`, `---`, `=`, `= ` and the like still underline (S023).
3. **A deliberate deviation from CommonMark**, kept narrow: the rendering of a file differs from
   other renderers only for an empty bullet with trailing blanks directly after paragraph text, which
   is mostly a line being typed. The file stays byte for byte as written.
4. **Theming baseline:** no new element and no new visible property; the empty item uses the bullet
   tokens of S003.

## Acceptance scenarios

**Scenario: indenting a new bullet under a bullet with text**
- Given `- first bullet` / `  - second bullet` / `    - ` with the caret on the last line, or
  elsewhere
- Then "second bullet" stays a bullet with its dot and no heading, and the last line is an empty
  item (a dot when concealed)

**Scenario: every marker with a blank**
- Given `Foo` / `- `, `Foo` / `* `, `Foo` / `+<tab>`, `- foo` / `  - ` or `  - second` / `    *   `
- Then the first line is no heading and the second is an empty item

**Scenario: the empty item ends the paragraph**
- Given `Foo` / `- ` / `---`
- Then `Foo` is no heading and `---` is a thematic break

**Scenario: a bare marker keeps CommonMark**
- Given `Foo` / `-`, `  - second` / `    -`, `Foo` / `-- ` or `Foo` / `=  `
- Then the line above is a setext heading as in S023; `Foo` / `*` / `---` is a two-line heading

## Tests

- **Core (`tests/core/test_s050_empty_bullet_after_text.cpp`):**
  `S050_EmptyBulletAfterText.IndentedEmptyBulletDoesNotUnderlineTheItemAbove`,
  `EveryMarkerWithABlankStartsAnItemAfterText`, `TheEmptyItemEndsTheParagraph`,
  `BareMarkerKeepsCommonMark`.
- `S023_ThematicBreaks.LinesThatCannotInterruptAParagraphContinueIt` now uses `Foo` / `+` / `===`
  instead of `Foo` / `* ` / `===`.
- The UI renders through the same core rules; `tst_s023_thematic_breaks` keeps comparing the UI with
  the whole-document reference.

## Implementation

- **Core:** `blocks::setextUnderline` (`src/core/src/block_rules.cpp`) rejects a single `-` followed
  by a blank; `BulletElement::startsAfterParagraph` (`src/core/src/elements/bullet.cpp`) lets a
  marker followed by a blank start an item after paragraph text.
