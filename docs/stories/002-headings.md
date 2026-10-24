# S002 — Headings render large, `#` hidden unless the cursor is on the line

- **Status:** Proposed · **Order:** 5 · **Depends on:** S001

## Story

As a writer, I want headings to look like headings while the `#` markers get out of my way, so
that the document reads like a rendered page but stays plain Markdown.

## Acceptance scenarios

**Scenario: heading rendered, markers hidden**
- Given the line `## Plans` and the caret on another line
- Then `Plans` is shown at heading level 2 size and weight, and `## ` is not visible

**Scenario: markers revealed on the cursor line**
- When the caret moves onto that line (keyboard or mouse)
- Then `## ` is visible in marker style; when the caret leaves, it is hidden again

**Scenario: selection reveals**
- When a selection includes any part of the heading line
- Then its markers are visible

**Scenario: levels and non-headings**
- Given lines `#` to `######` followed by a space, and `####### x`, `#x`, ` # x`
- Then levels 1–6 render as headings; the others are plain text (CommonMark rules for ATX
  headings that apply to the MVP; up to three leading spaces allowed)

**Scenario: only affected lines are re-rendered**
- When I type in or move between lines
- Then only the edited lines and the old/new caret lines are re-styled (asserted via a counter)

## Tests

- Core: `S002_Headings` – parse rule (levels, edge cases), render mapping
  `(line text, caret/selection on line?) → style runs + concealed ranges`.
- Core property test: for arbitrary text and caret positions the mapping never throws, ranges
  stay inside the line, and concealed ranges never cover the caret line.
- UI: `tst_s002_headings` – offscreen; caret moves; snapshot images of the three states
  (compare with tolerance; images stored per platform if fonts differ).

## Notes

- Introduces the **element registry**: an element = one parse rule + one render rule + its
  tests, registered with the engine. The editor loop knows only the registry.
- Concealment uses the ADR 0001 technique inside the Qt adapter; S009 decides whether it stays.
