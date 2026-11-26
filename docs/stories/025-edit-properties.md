# S025 — Editing properties in the properties block

- **Status:** In progress (confirmed 2026-11-21 by the user's request) ·
  **Order:** 44 · **Depends on:** S024
- **Basis:** The user supplied an Obsidian reference showing a compact, table-like property block
  with inline value fields, removable tag chips, and an Add property action. S024 shows properties
  and leaves editing to the YAML text.

## Story

As a writer who also uses Obsidian, I want to add, change and delete properties in the properties
block, so that I do not have to edit YAML for a date or a tag.

## Acceptance scenarios

### Editing a property

- Given a note with valid front matter displayed as properties, when the writer clicks a key or a
  text, number, date or date-time value, that cell becomes editable in place without revealing the
  YAML source or opening a dialog.
- Given a checkbox property, when the writer clicks its checkbox, its value toggles.
- Given a property key, when the writer clicks it, the key can be changed in place.
- Given a list or tags property, when the writer edits a chip, clicks its remove control, or uses
  the inline add chip, the change is reflected without revealing the YAML source.
- Given a nested or unsupported value, when the writer edits it, it remains editable as text rather
  than being silently discarded.

### Adding and deleting properties

- Given a displayed properties block, when the writer clicks the compact, menu-like Add property
  action, an inline key editor appears; committing a name adds a text property with an empty value.
- Given a displayed property, when the writer uses its inline remove control, only that property's
  source lines are removed.
- Given any edit, when the writer undoes it, the previous Markdown is restored in one undo step.
- Given an invalid front-matter block, its YAML remains visible and is not modified through the
  properties view.

### Source and formatting

- Each edit changes only the affected property's source range; unrelated YAML, comments, and the
  Markdown body remain intact.
- Newly entered scalar strings are quoted and escaped as YAML; list items remain distinct even when
  they contain commas, quotes, or line breaks.
- Valid front matter remains a graphical properties block regardless of caret, selection, or search
  position; clicking above the Properties title and moving Up from the first body line do not show
  or navigate into its YAML.
- Invalid YAML remains visible as text with a badge.
- No property edit opens a popup or modal dialog.
