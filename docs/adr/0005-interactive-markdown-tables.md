# ADR 0005: Interactive Markdown tables without raw syntax reveal

- **Status:** Accepted
- **Date:** 2026-10-26
- **Deciders:** project owner

## Context

The brief (section 1) defines a hybrid live-preview editing model where the document is always plain
Markdown text, rendered inline, with syntax markers generally revealed where the cursor is.

For GitHub Flavored Markdown (GFM) pipe tables, revealing raw pipe (`|`) and delimiter-row
(`| --- |`) syntax when the cursor enters a table forces the user to manage pipe characters, column
counts and separator rows by hand. In editors such as Obsidian (1.5+), tables are an intentional
exception to raw-syntax reveal: the table stays rendered as a visual grid of editable cells while
the user types directly inside the fields and adds rows or columns graphically, and the underlying
Markdown text is updated automatically.

## Decision

1. **Table syntax is never revealed.** Pipe delimiters (`|`), surrounding cell-padding whitespace,
   escaped-pipe backslashes (`\|`) and the delimiter row (`| --- | :---: | ---: |`) remain
   concealed even when the caret or selection is inside the table.
2. **One copy of the text (ADR 0002) still holds.** Table rows remain ordinary lines (`QTextBlock`s)
   of GFM pipe-table Markdown inside the editor's `QTextDocument`. No separate table document model
   is kept.
3. **Cell-addressed layout and editing.**
   - `core` parses table headers (with one-line lookahead to the delimiter row), delimiter rows
     (recording per-column alignment: left, centre, right) and body rows into `TableCell` spans,
     runs inline elements independently within each cell, and provides pure helpers to add/remove
     rows, add/remove columns and normalise cell counts in Markdown text.
   - `ui` measures natural column widths across each contiguous table block and caps total table
     width to the available content display width (preserving narrow columns while wrapping wide
     ones), lays out each cell into its own column box via `QTextLayout` so wrapped lines stay
     within their column's horizontal bounds, paints the table panel, header background, alternating
     row stripes, cell grid lines and hover-revealed graphical `+` / `−` controls (plus buttons to
     add rows and columns, a per-body-row minus button in the left gutter to delete a row, and a
     per-column minus button above each column header to delete a column, as well as context-menu
     `Add Row` / `Delete Row` / `Add Column` / `Delete Column` actions), confines typing, deletion
     (`Backspace`/`Delete`, which delete a body row when all of its cells are empty or a column when
     all of its cells are empty, or via `Ctrl+Shift+Backspace` / `Ctrl+Alt+Backspace`), arrow
     navigation and `Tab`/`Shift+Tab` to the table cells, and lets the user exit the table
     intuitively (pressing `Enter` on an empty last row removes that empty row and moves below the
     table; `Ctrl+Enter`, `Escape` or clicking below/above the table also moves the caret onto a
     plain line outside the table).
4. **Element theme tokens (ADR 0004).** Tables have a `[table]` section with tokens and fallbacks:

| Token | Type | Fallback |
|---|---|---|
| `table.background` | color | `colors.background` |
| `table.header-background` | color | `colors.surface` |
| `table.stripe-background` | color | `table.background` |
| `table.text` | color | `colors.text` |
| `table.header-text` | color | `table.text` |
| `table.header-weight` | number, 100 to 900 | `headings.weight` |
| `table.border` | color | `colors.border` |
| `table.border-width` | number (px), 0 to 8 | 1 |
| `table.radius` | number (px), 0 to 32 | `layout.radius` |
| `table.padding` | number (px), 0 to 64 | `layout.spacing` |
| `table.font-family` | string | `""` = editor font |
| `table.font-scale` | number, 0.5 to 2 | 1 |

## Consequences

- Users edit table fields directly and add rows or columns graphically without touching `|` or
  `| --- |` syntax.
- Saving (S004) writes valid GFM pipe-table Markdown byte for byte; untouched tables are preserved
  unchanged.
- Undo and redo work natively through `QTextDocument`.

## Reconsider if

- Users need multi-line block constructs inside individual table cells (which GFM pipe tables do not
  support).
- Users ask for a dedicated "source mode" toggle to edit raw table Markdown directly.
