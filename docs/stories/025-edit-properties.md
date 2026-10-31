# S025 — Editing properties in the properties block

- **Status:** Open (recorded 2026-10-30 from Armin's decision on S024, not yet refined) ·
  **Order:** 26 · **Depends on:** S024
- **Basis:** Obsidian edits properties in the block itself: a row per property with a name and a
  value field, "Add property" below the rows, a type menu behind the icon, checkboxes that tick
  (Obsidian help, "Properties"). S024 shows properties and leaves editing to the YAML text.

## Story

As a writer who also uses Obsidian, I want to add, change and delete properties in the properties
block, so that I do not have to edit YAML for a date or a tag.

## To refine before confirmation

- Which edits: "Add property", key and value fields, ticking a checkbox, a date picker, adding and
  removing list items, deleting a row, changing a type.
- How edits reach the file: each edit replaces only the bytes of one key or value (ADR 0002), using
  the lines the parser of ADR 0007 reports; how a new value is quoted.
- Keyboard: Obsidian's keys (Up/Down, Tab, Alt+Down into the editor, Ctrl+; to add) against
  hitokoto's caret moving into the YAML (S024).
- Undo: one step per edit in the document's undo stack.
- Invalid YAML and values of unsupported type: read only or editable as text.
