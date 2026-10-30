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

**Scenario: extension by addition (brief, section 5)**
- Given the S002 heading element
- When the bullet element is added
- Then no file of the heading element and no file of the editor loop changes, except the one
  registration line (checked in review via the PR diff and documented in the PR)

## Tests

- Core: `S003_Bullets` – parse rule, render mapping and `wrapIndent`.
- Core: registry test with both elements on mixed documents.
- UI: `tst_s003_bullets` – snapshot of dot vs. raw marker, wrapped continuation line alignment.

## Notes

- `---` (thematic break) and numbered lists are out of scope; they must simply stay plain text.
