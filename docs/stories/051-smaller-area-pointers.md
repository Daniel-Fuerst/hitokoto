# S051 — Smaller pointers for the areas of rows and columns

- **Status:** Confirmed (Armin, 2026-11-27, at the acceptance of S040) · **Order:** 56 ·
  **Size:** S · **Depends on:** S040
- **Basis:** Armin, 2026-11-27, v0.12.0 on Windows: the pointers over the areas of rows and columns
  (S040, decision 6) work but look large. Measured in the code (`TableWidget::areaPointer`,
  `icons/select-row.svg`, `icons/select-column.svg`): each arrow spans 28 × 20 of a 32 × 32 canvas
  at a scale of 1, twice that at 2x. Windows' standard arrow fills about 12 × 19 of its 32 × 32
  canvas, so hitokoto's arrow is about twice as wide.

## Story

As a writer, I want the pointers over the areas of rows and columns about as large as the system's
own pointers, so that they show where a click selects without standing out.

## Decisions (Armin, 2026-11-27)

1. **Two thirds of today's size.** Each arrow spans about 19 × 13 of the 32 × 32 canvas at a scale
   of 1 (the row's pointing right, the column's pointing down), with the same shape, the same outline
   and the theme's colours (`table.select-pointer-fill`, `table.select-pointer-outline`).
2. **The hot spot stays at the tip**, where the arrow points into the area.
3. **No following of the system's pointer size** in this story.

## Acceptance scenarios

**Scenario: the pointer of a row's area is smaller**
- Given a table and the mouse over the area of a row
- Then the pointer is an arrow pointing right, about 19 pixels long and 13 high at a scale of 1, its
  tip at the hot spot, in the theme's pointer colours
- And at a scale of 2 it is twice as large in pixels and as sharp

**Scenario: the pointer of a column's area is smaller**
- Given a table and the mouse over the area of a column
- Then the pointer is an arrow pointing down, about 13 pixels wide and 19 high, its tip at the hot spot

## Theming

No new element: the pointers keep the tokens of S040.

## Tests

- UI: `tst_s051_smaller_area_pointers` (or an extension of `tst_s040_table_selection`): the extent of
  the opaque pixels of both pointers at a scale of 1 and 2, the hot spot at the tip, the colours.
