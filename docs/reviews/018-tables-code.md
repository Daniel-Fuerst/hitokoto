---
mode: code
spec: docs/stories/018-tables.md  # S017 when reviewed, renumbered on 2026-10-27
related: docs/adr/0005-interactive-markdown-tables.md
branch: feature/tables
commit_range: bdfd255..a3def18
reviewed_on: 2026-10-27
dispositioned: 2026-10-27 (Armin, after the rework on feature/tables up to ca9c8ff and Daniel's and Armin's test of it)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), presets dev, ci, core-sanitize
objections:
  - id: O1
    category: premise
    severity: major
    title: Unescaped pipes inside code spans do not split cells, unlike GFM
    disposition: fixed
    disposition_rationale: "The table follows GFM: every unescaped pipe splits cells, also inside a code span (checked against cmark-gfm); test S018_Tables.PipesInCodeSpansSeparateCellsLikeGfm (story revision, Armin's decision)."
  - id: O2
    category: failure
    severity: major
    title: One keystroke in a 50-row table starts an endless scroll-bar/relayout loop
    disposition: fixed
    disposition_rationale: "The table is an embedded table view whose width does not depend on the scroll bar (S019 layout, ADR 0005 revised); tests tableWidthDoesNotDependOnScrollBarVisibility and typingInATableNearTheScrollBarThresholdSettles (red in 79ef028)."
  - id: O3
    category: failure
    severity: major
    title: Typing latency grows with the table; 200 rows break the 16 ms and 50 ms budgets
    disposition: fixed
    disposition_rationale: "A keystroke changes one line and fits the view with cached cell heights; benchmark s018_table_typing_* on a 200-row table (development VM, every keystroke written to the line: p95 4.2 ms; hard budget 50 ms per key)."
  - id: O4
    category: failure
    severity: major
    title: Typing over a selection that spans cells deletes pipes and merges cells
    disposition: fixed
    disposition_rationale: "Cells are edited in a cell editor; a selection cannot cross cell boundaries, so typing over it cannot delete pipes; test editingACellChangesOnlyThatCell."
  - id: O5
    category: failure
    severity: major
    title: Paste into a cell inserts raw pipes and line breaks
    disposition: fixed
    disposition_rationale: "Pasted pipes are written as \\| and line breaks as <br>; test editingACellChangesOnlyThatCell."
  - id: O6
    category: failure
    severity: major
    title: Navigation and Escape change the document
    disposition: fixed
    disposition_rationale: "Moving with keys, clicks and Tab never changes the file and short rows are not padded; test movingAroundNeverChangesTheFile. Leaving at the end of the document adds a blank line and a line by the confirmed story (first version behaviour)."
  - id: O7
    category: design
    severity: major
    title: Excess cells are hidden for good and cannot be reached
    disposition: fixed
    disposition_rationale: "Excess cells are marked in the table view, and the source view (Ctrl+Shift+M) shows and edits all Markdown lines; test sourceViewShowsTheMarkdownLines."
  - id: O8
    category: design
    severity: major
    title: Per-cell layout overrides QPlainTextDocumentLayout ahead of ADR 0003
    disposition: fixed
    disposition_rationale: "Per-cell layout is gone: S019 replaced the editor surface with QTextEdit and BlockLayout (ADR 0003, accepted), the table is an embedded QTableView (ADR 0005 revised)."
  - id: O9
    category: operational
    severity: major
    title: Story number S017 is taken on main; rebase has three conflicts, one semantic
    disposition: fixed
    disposition_rationale: "Story renumbered to S018 (3b1297a), main merged into feature/tables with the semantic conflict in element_registry.cpp resolved (2b34295); the old commit messages keep S017 (merge, no rebase)."
  - id: O10
    category: operational
    severity: minor
    title: No benchmark for tables
    disposition: fixed
    disposition_rationale: "Benchmarks s018_table_open_ms (1000 rows) and s018_table_typing_* (200 rows) in hitokoto-bench, with a hard budget of 50 ms per key."
  - id: O11
    category: operational
    severity: minor
    title: S009 quality gate not extended for tables
    disposition: fixed
    disposition_rationale: "S009 has a table scenario (caret, input method and screen reader in cells)."
  - id: O12
    category: operational
    severity: minor
    title: clang-format-18 (CI job Format) fails on theme.cpp
    disposition: fixed
    disposition_rationale: "theme.cpp back to Theme::*Member; clang-format-18 over the whole repository without findings (ed34fbd)."
---

# Objections: S017 Markdown tables (feature/tables, code mode)

The story is S018 since 2026-10-27 (objection O9); the text below keeps the numbers of the review.

Reviewed: `bdfd255..a3def18` (two commits by Daniel, 2026-10-26 and 2026-10-27), branch not rebased
onto main (32 commits behind, main at 88edbe1). Build and tests: presets `dev` and `ci` build without
warnings, all 112 tests pass, `core-sanitize` 98/98, clang-tidy-18 over the ten changed source and test files reports nothing. The fuzzers were not run. The findings below come from reading the code,
cmark-gfm 0.29.0.gfm (Python `cmarkgfm` 2025.10.22) as GFM reference, and probe tests run against
the branch (not committed; listed under each objection).

## O1 - premise - major

**Unescaped pipes inside code spans do not split cells, unlike GFM.** GFM splits a row into cells
before inline parsing; a pipe inside a code span must be escaped. cmark-gfm renders
`` | `x | y` | z | `` (header with two columns) as the cells `` `x `` and `` y` ``. The branch skips
code spans when scanning pipes (`src/core/src/elements/table.cpp:124-130`), the story requires it
("pipes inside inline code … do not split cells", scenario "inline Markdown and escaped pipes"), and
the test `S017_Tables.EscapedPipesAndInlineCodePipesDoNotSplitCells`
(`tests/core/test_s017_tables.cpp:115-135`) locks it in. The same file therefore shows a different
table in hitokoto than on GitHub, and the cell guards protect the wrong structure. The story claims
"GFM 4.10" as scope.

## O2 - failure - major

**One keystroke in a 50-row table starts an endless scroll-bar/relayout loop.** Probe: table with a
header and 50 rows of five columns, window 900×700 (test fixture), caret in row 25, type one
character. Afterwards the viewport received 16,356 resize events within 44 s, the event loop never
became idle; the test watchdog killed the run at 300 s (Debug and Release). Backtrace:
`QAbstractScrollAreaPrivate::layoutChildren_helper` → `EditorWidget::resizeEvent`
(`src/ui/editor_widget.cpp:164`) → `BlockStyler::setAvailableWidth`
(`src/ui/block_styler.cpp:114-127`), which unstyles every table row and restyles them. The table
width depends on the viewport width, the viewport width on the scroll bar, the scroll bar on the
document height, and the height on the table wrapping: with the bar shown the table wraps and needs
the bar, without it the table is shorter and does not. The UI hangs; nothing bounds the loop.

## O3 - failure - major

**Typing latency grows with the table; 200 rows break the 16 ms and 50 ms budgets.** Probe (Release,
`ci` preset, mean/max of 20 keystrokes including `processEvents`): 10 rows 5.0/5.7 ms, 25 rows
7.7/8.6 ms, 200 rows 50.3/62.9 ms, 400 rows 46.7/52.3 ms; 400 lines of plain text 0.35/0.46 ms.
Cause: every `contentsChange` clears the table metrics (`block_styler.cpp:478`, `:511`), unstyles all
rows of the table (`:497-508`) and re-measures every row via `renderFor` and `cellTextWidth`, then
`layoutTableRow` lays out every row again. The brief (section 4) sets one frame (~16 ms) for
keystroke to screen and asks to re-render only what changed; the S017 gate on main has a hard budget
of 50 ms per keystroke. Numbers are from a VM, but the growth with row count is the point.

## O4 - failure - major

**Typing over a selection that spans cells deletes pipes and merges cells.** Only `Backspace` and
`Delete` clamp a selection to the cell (`src/ui/table_controller.cpp:1085-1105`, `:1141-1161`).
Shift+arrows are not confined, and a printed character falls through to
`QPlainTextEdit::keyPressEvent`. Probe: `| ab | cd |`, caret between `a` and `b`, five times
Shift+Right selects `b | c`, typing `x` gives `| axd |`: the row now has one cell, the column is
lost. Cut (Ctrl+X) and IME commits are not guarded either (not probed).

## O5 - failure - major

**Paste into a cell inserts raw pipes and line breaks.** No `insertFromMimeData` override exists in
`src/ui`; only a typed `|` is escaped (`table_controller.cpp:1166-1172`). Probe: clipboard
`x|y` + newline + `z` pasted after `a` in `| ab | cd |` gives `| ax|y` and `zb | cd |`: the row is
split into two lines and gains a cell. The story scenario "typing `|` inside a cell inserts `\|`"
holds only for the keyboard.

## O6 - failure - major

**Navigation and Escape change the document.** `placeCaretInCell` pads a row that has fewer cells
than the target column (`table_controller.cpp:383-392`); `exitTableBelow` inserts `\n\n` when the
table ends the document or is followed directly by text (`:572-580`), `exitTableAbove` likewise at
the top (`:610`). Probes: `| a |` under a three-column header, Tab → text becomes `| a |  |`,
`isModified()` true; Escape in a table at the end of the file appends `\n\n`, `isModified()` true.
Short rows are valid GFM. ADR 0005 states "untouched tables are preserved unchanged"; here moving
the caret is enough to change bytes, set the unsaved-changes state (S008) and create a diff.

## O7 - design - major

**Excess cells are hidden for good and cannot be reached.** `render` keeps at most as many cells as
the header has and collapses the rest of the line (`table.cpp:325`, `:346-349`). Probe:
`| c | d | secret |` under a two-column header: `secret` is collapsed; End, Tab, End, Right moves
past it to the next line. GFM also drops excess cells when rendering, but an editor that never
reveals table syntax (ADR 0005, decision 1) leaves no way to see or fix text that is in the file.
Deleting the row or column deletes it unseen. The same applies to anything else the table hides
permanently (indentation, trailing text after the last pipe).

## O8 - design - major

**Per-cell layout overrides QPlainTextDocumentLayout ahead of ADR 0003.** `layoutTableRow` lays out
the `QTextLine`s of a block itself, in column order rather than reading order, and then calls
`block.setLineCount(1)` (`block_styler.cpp:283-365`). `QPlainTextDocumentLayout` re-lays blocks on
its own; the branch detects that through `leadingIncluded()` of the first line
(`block_styler.cpp:274-279`), an undocumented side effect. Caret movement up/down, hit testing,
IME and accessibility all depend on line order and are reimplemented in `TableController`. S009
exists to decide (ADR 0003) whether the 1-px adapter on `QPlainTextEdit` stays or is replaced by a
custom text surface "before more elements depend on it"; this branch makes the adapter much harder
to replace. O2 is one symptom.

## O9 - operational - major

**Story number S017 is taken on main; rebase has three conflicts, one semantic.** Main has S017
"Saving never stalls the UI" (story file, README order 18, `tst_s017_save_without_stall`,
`docs/reviews/017-save-without-stall-code.md`). The branch uses S017 and order 18 for tables in
`docs/stories/017-tables.md`, `docs/stories/README.md`, `docs/architecture.md`, `table.cpp`,
`tests/core/test_s017_tables.cpp` (suite `S017_Tables`), `tests/ui/tst_s017_tables.cpp`, both
`CMakeLists.txt` and both commit messages. A trial merge of main gives conflicts in
`docs/stories/README.md`, `tests/ui/CMakeLists.txt` and `src/core/src/element_registry.cpp`. The
last one is not textual only: main's S013 fix O3 made `splitAroundInlineRuns` position-based
(`firstInlineRun`), while the branch's per-cell path (`element_registry.cpp:180-206`) calls the old role-based variant. With merge
commits (Daniel's rule), the commit messages naming S017 stay in main's history; a rebase can fix
them before merging.

## O10 - operational - minor

**No benchmark for tables.** The branch does not touch `bench/`. The brief (section 4) asks for
benchmarks of typing latency and large-file scenarios; the S017 gate on main fails only on what it
measures. O3 would not have shown up in CI.

## O11 - operational - minor

**S009 quality gate not extended for tables.** CLAUDE.md and `docs/stories/README.md`: S009's
scenarios "are extended whenever an element conceals syntax". Tables conceal more than any element
so far (whole delimiter rows, padding, backslashes) and never reveal; `docs/stories/009-…` is
unchanged. Caret-never-inside-hidden-text, IME and screen reader in cells are not covered.

## O12 - operational - minor

**clang-format-18 (CI job Format) fails on theme.cpp.** The branch changes `Theme::*Member` to
`Theme::* Member` in three templates (`src/core/src/theme.cpp:61`, `:87`, `:111`), apparently
formatted with a newer clang-format. `clang-format-18 --dry-run --Werror` (as in `ci.yml`) reports
three violations; main has none.

## Capped concerns

- Tables longer than 400 rows get different column widths per part: metrics walk at most
  `kSynchronousLimit` rows (`block_styler.cpp:190`, `:201`); probe 600 rows: row 10 column width 33,
  row 560 296, the `v` in column 2 at x 89 vs 352.
- Alignment is packed for 12 columns only (`table.cpp:20`, `:240`); probe: column 14 marked `---:`
  renders left. A row with more than 63 internal pipes ends the table (`table.cpp:139-141`); GFM
  ignores excess cells.
- A body line without a pipe ends the table (`table.cpp:302-305`); cmark-gfm keeps it as a row
  (`bar` → cells `bar`, empty) until a blank line or another block starts.
- `\|` inside a code span: GitHub shows `|` in the code, hitokoto shows the backslash, because
  `hideEscapedPipeBackslashes` skips code spans (`table.cpp:250-255`).
- Backspace or Delete in an empty cell deletes the whole column when every cell of it is empty
  (`table_controller.cpp:1072`, `:1128`), also in a column just added.
- `exitTableBelow` moves to the line after an existing blank line (`table_controller.cpp:557-561`),
  which may be a heading or the next table rather than a plain line.

## Explicitly not objecting to

- Tables as a story at all: it is a documented scope extension with its own story and ADR, as the
  conventions require.
- One copy of the text (ADR 0002): rows stay plain Markdown lines in the `QTextDocument`, no shadow
  model.
- Table core without Qt and its tests: `hitokoto_core` stays standard-library only, the parser does
  not allocate in `blockStateAfter`, `core-sanitize` is clean.
- Theme tokens: the `[table]` section with fallbacks follows ADR 0004 and is documented in
  `docs/theming.md`.
- Undo: row and column operations use one edit block each.
- The refactoring in a3def18 (TableController/TablePainter out of EditorWidget, no mutations in
  `paintEvent`, `BatchScope` for column operations).
