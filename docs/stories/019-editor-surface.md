# S019 — Editor surface with its own document layout

- **Status:** In review · **Order:** 19 · **Depends on:** S017
- **Basis:** S018 review (O2, O8) and the decision of 2026-10-27 to implement plan B of ADR 0001
  directly on `feature/tables`; ADR 0003. Confirmed by the user on 2026-10-27.
- **Review basis:** Linux only (development VM, Qt 6.12.0, presets dev and ci, benchmarks in
  `docs/performance.md`). The user waived the Windows and macOS CI runs on 2026-10-27; they run
  with the next pull request to `main`.

The tables story needs blocks that reserve space for an embedded table and pixel-based scrolling.
`QPlainTextEdit` offers neither (ADR 0003). This story replaces the editor surface without
changing what the user sees; S018 then builds the tables on it.

## Story

As a writer, I want the editor to keep behaving exactly as it does now while it moves to a
surface that can hold tables, so that tables can come without a step back elsewhere.

## Acceptance scenarios

**Scenario: nothing changes for existing elements**
- Given any document the existing stories cover (S001–S017)
- When I open, type, click, select, undo, save and switch themes
- Then every existing scenario still passes unchanged, including S009's caret rules and S010's
  click on concealed lines

**Scenario: budgets hold**
- Given the benchmarks of S006, S007 and S017
- When they run on the new surface
- Then cold start, opening 10 MB, typing latency (p95 ≤ 16 ms, no keystroke over 50 ms) and the
  save stall stay within the brief's budgets and the CI gate

**Scenario: scrolling is pixel-based and settles**
- Given a document whose height is close to the viewport height
- When typing changes whether the vertical scroll bar is needed
- Then the scroll bar is shown or hidden once and the layout settles (S018 tests for O2 pass)

**Scenario: a block can reserve space or be hidden**
- Given a block marked to reserve a height H, and blocks marked hidden
- When the document is laid out
- Then the marked block is H pixels high, hidden blocks take no space, the document height and
  scroll range include exactly that, and the caret never lands in a hidden block

**Scenario: input methods and accessibility come from Qt**
- Given a heading line and a bullet item
- When an input method composes text (pre-edit) and commits it
- Then the pre-edit appears at the caret and the committed text lands at the caret position
- And the editor exposes its text through Qt's accessibility interface for text edits

## Scope

- `EditorWidget` on `QTextEdit` (`setAcceptRichText(false)`), `BlockLayout` as its document layout.
- `BlockStyler`, painting of panels and decorations, and S010 click handling moved onto
  `BlockLayout` geometry.
- Not in scope: tables themselves (S018), changes to concealment (S009), new theme tokens.

## Tests

- UI: `tst_s019_editor_surface` for pixel scrolling, reserved and hidden blocks, input method
  pre-edit and commit, accessibility interface.
- All existing core and UI tests and the benchmark gate unchanged.
