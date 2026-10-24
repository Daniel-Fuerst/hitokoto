# S010 — Clicking a line with hidden markers puts the caret where clicked

- **Status:** Done in v0.1 · **Order:** 9 · **Depends on:** S009

**Implementation note:** Done (v0.1, 2026-10-24). Tests: UI `tst_s010_click`.

## Story

As a writer, I want a click into a heading or bullet line to put the caret on the character I
clicked, even though revealing the markers shifts the text, so that mouse editing feels precise.

## Acceptance scenarios

**Scenario: click into a heading**
- Given `## Plans` with hidden markers
- When I click between `P` and `l`
- Then the markers are revealed and the caret sits between `P` and `l` (not two characters left)

**Scenario: drag selection starting on a concealed line**
- When I press on a concealed line and drag to another line
- Then the selection starts at the clicked character

## Tests

- UI: `tst_s010_click` – synthesised mouse events offscreen, caret position asserted.

## Notes

- Obsidian keeps the clicked character under the caret and accepts the visual shift; the same is
  proposed here. Whether the line should also be scrolled to keep the pointer over the clicked
  character is a UX question for this story.
