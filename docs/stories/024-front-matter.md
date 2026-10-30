# S024 — Front matter shown as properties

- **Status:** Open (recorded 2026-10-30 from review objection O2 of S023, not yet refined) ·
  **Order:** 25 · **Depends on:** S023
- **Basis:** Obsidian stores note properties as YAML between a `---` on the first line and the next
  `---`. CommonMark and GFM do not know front matter. Since S023 hitokoto recognises such a block
  and leaves it as plain text, so that it neither turns into a horizontal line nor a setext heading.

## Story

As a writer who also uses Obsidian, I want the front matter at the top of a note shown as a compact
block of properties, so that it does not look like body text, while the YAML stays byte for byte.

## To refine before confirmation

- Look: a properties block (key and value per row) or a styled YAML block like a code panel; its
  theme tokens (ADR 0004).
- Editing: in place as text when the caret enters, or through a table like S018.
- Invalid YAML, empty front matter (`---` / `---`), `...` as the closing line, a byte order mark
  before the first `---`.
- Recognition stays as in S023 (first line `---`, closed by the next `---`).
