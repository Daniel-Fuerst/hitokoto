# S003 — Bullets render as dots, through the same element mechanism

- **Status:** Done in v0.1 · **Order:** 6 · **Depends on:** S002

**Implementation note:** Done (v0.1, 2026-10-24). Nested items alternate filled and hollow dots. The bullet element was added with one registration line (`ElementRegistry::withBuiltins`). Tests: core `S003_Bullets`, UI `tst_s003_bullets`.

## Story

As a writer, I want list items to show a bullet dot instead of `-`, `*` or `+`, so that lists read
naturally while the source stays plain Markdown.

## Acceptance scenarios

**Scenario: bullet rendered**
- Given the line `- milk` and the caret elsewhere
- Then a small dot in marker/accent colour is shown in place of `- `, and `milk` is aligned after it

**Scenario: raw marker on the cursor line**
- When the caret is on that line
- Then the raw `- ` is shown and no dot is drawn

**Scenario: all three markers, nesting as plain indentation**
- Given lines starting with `- `, `* `, `+ ` (optionally indented)
- Then each renders a dot at its indentation level; `-x` and `--- ` are not list items

**Scenario: wrapped lines have hanging indent**
- Given a list item whose text wraps across multiple visual lines
- Then the continuation lines start aligned with the item's text (hanging indent), whether the marker is concealed or revealed

**Scenario: Enter continues the bullet list**
- Given a list item with text (e.g. `- milk` or `  * milk`)
- When I press Enter at or after the marker
- Then a new list item with the same marker and indentation is created on the next line

**Scenario: Enter on an empty bullet outdents or exits the list**
- Given an empty bullet item with no text
- When I press Enter
- Then a nested item is outdented by one level, and a top-level item's marker is removed leaving a blank line

**Scenario: Tab indents a bullet item from anywhere on the line**
- Given the caret anywhere on a bullet item line
- When I press Tab
- Then the item is indented by one level (2 spaces), and the caret position follows the text

**Scenario: Shift+Tab outdents a bullet item from anywhere on the line**
- Given the caret anywhere on an indented bullet item line
- When I press Shift+Tab
- Then the item is outdented by one level (2 spaces), and the caret position follows the text

**Scenario: default slight indent for bullet items**
- Given a bullet list item and a normal paragraph
- Then the bullet list item has a slight default indent compared to the paragraph

**Scenario: additional separation between bullet dot and text**
- Given a bullet list item (e.g. `- milk`)
- Then the space between the bullet dot and the text behind it is wider than a standard space, creating additional visual separation

**Scenario: extension by addition (brief, section 5)**
- Given the S002 heading element
- When the bullet element is added
- Then no file of the heading element and no file of the editor loop changes, except the one
  registration line (checked in review via the PR diff and documented in the PR)

## Tests

- Core: `S003_Bullets` – parse rule, render mapping, `wrapIndent`, bullet line parsing and indentation.
- Core: registry test with both elements on mixed documents.
- UI: `tst_s003_bullets` – snapshot of dot vs. raw marker, wrapped continuation line alignment, Enter auto-continuation/exit, Tab/Shift+Tab indentation, default slight indent, extra separation after bullet dot.

## Notes

- `---` (thematic break) and numbered lists are out of scope; they must simply stay plain text.
  Changed by S023 (2026-10-30): thematic breaks render as a horizontal line; they are still no items.
  Directly after a line of text, a bullet without text and a bullet indented four or more columns
  past the text's content column are text that continues the paragraph (CommonMark 5.2, S023 review
  O3, code).
