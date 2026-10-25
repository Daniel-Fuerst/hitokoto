# MVP story backlog

- **Status:** Confirmed 2026-10-24; v0.1 implements S000–S008 and S010 (S007 partly)
- **Date:** 2026-10-24
- **Basis:** `docs/brief.md`, `docs/adr/0001-technology-stack.md` (C++20, Qt 6 Widgets, CMake)

Story IDs S001–S007 keep the numbers from the brief; S000 and S008–S012 are new. The order below
is the proposed implementation order. Each story is one branch/PR, implemented test-first.
Tests reference the story ID (see "Conventions").

| Order | ID | Story | Depends on | Size | Status |
|---|---|---|---|---|---|
| 1 | [S000](000-repository-skeleton.md) | Repository skeleton, build and CI on three OSes | – | M | Done |
| 2 | [S001](001-open-file.md) | Opening a Markdown file shows its content | S000 | M | Done |
| 3 | [S004](004-save-byte-exact.md) | Save and Save As write the text byte for byte, atomically | S001 | M | Done |
| 4 | [S008](008-unsaved-changes.md) | Unsaved changes are never lost silently | S004 | S | Done |
| 5 | [S002](002-headings.md) | Headings render large, `#` hidden unless the cursor is on the line | S001 | L | Done |
| 6 | [S003](003-bullets.md) | Bullets render as dots, through the same element mechanism | S002 | M | Done |
| 7 | [S006](006-cold-start-budget.md) | Cold start within budget, measured in CI | S003 | M | Done (baselines from CI pending) |
| 8 | [S009](009-concealment-quality-gate.md) | Concealment quality gate: caret, IME, accessibility | S003 | M | Open |
| 9 | [S010](010-click-on-concealed-line.md) | Clicking a line with hidden markers puts the caret where clicked | S009 | S | Done |
| 10 | [S005](005-theme-files.md) | A theme file changes the look; light and dark built in | S003 | L | Done |
| 11 | [S007](007-large-files.md) | 10 MB files open within budget and type without lag | S006 | L | Partly done |
| 12 | [S011](011-reference-hardware.md) | Budgets confirmed on reference hardware | S007 | S | Open |
| 13 | [S012](012-distributable-builds.md) | Distributable builds for Linux, Windows and macOS | S000 | M | Open |
| 14 | [S013](013-inline-emphasis.md) | Inline bold, italic and strikethrough | S003 | M | In Progress |

Sizes: S = up to a day, M = a few days, L = about a week of part-time work (rough).

"Done" in v0.1 means: all scenarios pass locally on Linux with Qt 6.12 and Qt 6.8. Windows and
macOS are covered by the CI workflow, which has not run yet; the first CI run confirms or reopens
these stories.

## Decisions (confirmed 2026-10-24)

1. **Licence:** GPL-3.0-or-later (`LICENSE`). Qt is used under LGPLv3 (dynamic linking).
2. **Minimum platforms:** Windows 10 22H2 and 11 (x64), macOS 13 or newer, Linux with Qt 6.8 or
   newer (Debian trixie). CI builds with Qt 6.12 and, on Linux, with Qt 6.8 as the minimum.
3. **Text buffer ownership:** ADR 0002 – the editor's `QTextDocument` is the only copy of the text.
4. **Encoding and line endings:** ADR 0002 – UTF-8 only, invalid files refused; LF, CRLF and CR
   preserved; mixed files keep each line's ending, new lines take the majority.

## Conventions

- Story files: `docs/stories/NNN-title.md`, written and confirmed before implementation.
- Core tests (GoogleTest): test suite named after the story, e.g. `TEST(S002_Headings, MarkerHiddenWhenCursorElsewhere)`.
- UI tests (Qt Test, `QT_QPA_PLATFORM=offscreen`): one test class per story, e.g. `tst_s002_headings`.
- Benchmarks (Google Benchmark and the startup/latency harness from S006): names start with the story ID.
- A story is done when its scenarios pass on all three CI platforms, nothing regressed, and the
  performance budgets still hold.

## Not in the MVP (from the brief, kept in mind)

Inline code, links, task and numbered lists, blockquotes, code blocks, tables, images, search,
spellcheck, export, wiki links, multiple documents, plugins, keybinding schemes, localisation.
The element registry (S002/S003) and theme tokens (S005) are designed so these can be added
without changing existing elements.
