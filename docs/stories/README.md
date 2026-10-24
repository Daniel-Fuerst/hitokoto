# MVP story backlog

- **Status:** Proposed, waiting for confirmation (brief, section 10, step 2)
- **Date:** 2026-10-24
- **Basis:** `docs/brief.md`, `docs/adr/0001-technology-stack.md` (C++20, Qt 6 Widgets, CMake)

Story IDs S001–S007 keep the numbers from the brief; S000 and S008–S012 are new. The order below
is the proposed implementation order. Each story is one branch/PR, implemented test-first.
Tests reference the story ID (see "Conventions").

| Order | ID | Story | Depends on | Size |
|---|---|---|---|---|
| 1 | [S000](000-repository-skeleton.md) | Repository skeleton, build and CI on three OSes | – | M |
| 2 | [S001](001-open-file.md) | Opening a Markdown file shows its content | S000 | M |
| 3 | [S004](004-save-byte-exact.md) | Save and Save As write the text byte for byte, atomically | S001 | M |
| 4 | [S008](008-unsaved-changes.md) | Unsaved changes are never lost silently | S004 | S |
| 5 | [S002](002-headings.md) | Headings render large, `#` hidden unless the cursor is on the line | S001 | L |
| 6 | [S003](003-bullets.md) | Bullets render as dots, through the same element mechanism | S002 | M |
| 7 | [S006](006-cold-start-budget.md) | Cold start within budget, measured in CI | S003 | M |
| 8 | [S009](009-concealment-quality-gate.md) | Concealment quality gate: caret, IME, accessibility | S003 | M |
| 9 | [S010](010-click-on-concealed-line.md) | Clicking a line with hidden markers puts the caret where clicked | S009 | S |
| 10 | [S005](005-theme-files.md) | A theme file changes the look; light and dark built in | S003 | L |
| 11 | [S007](007-large-files.md) | 10 MB files open within budget and type without lag | S006 | L |
| 12 | [S011](011-reference-hardware.md) | Budgets confirmed on reference hardware | S007 | S |
| 13 | [S012](012-distributable-builds.md) | Distributable builds for Linux, Windows and macOS | S000 | M |

Sizes: S = up to a day, M = a few days, L = about a week of part-time work (rough).

## Decisions needed before or during the backlog

1. **Licence of hitokoto.** The repository has no licence yet. Qt is used under LGPLv3 (dynamic
   linking), which allows any licence for hitokoto itself. Needed before S012 at the latest.
2. **Minimum platforms.** Proposal: Windows 10 22H2 and 11 (x64), macOS 13 or newer (Apple
   Silicon and Intel), Linux with Qt ≥ 6.8 (Debian trixie, Ubuntu 24.04 with bundled Qt). To be
   checked against the Qt 6.12 supported-platforms list in S000.
3. **Text buffer ownership (ADR 0002, in S001).** Proposal: at runtime the editor's
   `QTextDocument` is the only copy of the text; `core` stays stateless over line strings and owns
   file I/O (bytes ↔ text, encoding, BOM, line endings). One copy cannot drift, which is what the
   brief asks for. Alternative: core owns a piece table and the editor mirrors it.
4. **Encoding and line endings (in S001/S004).** Proposal: UTF-8 only in the MVP (with or
   without BOM); invalid UTF-8 is refused with a clear message instead of being "repaired"; LF and
   CRLF are preserved; mixed line endings see "S004".

## Conventions

- Story files: `docs/stories/NNN-title.md`, written and confirmed before implementation.
- Core tests (GoogleTest): test suite named after the story, e.g. `TEST(S002_Headings, MarkerHiddenWhenCursorElsewhere)`.
- UI tests (Qt Test, `QT_QPA_PLATFORM=offscreen`): one test class per story, e.g. `tst_s002_headings`.
- Benchmarks (Google Benchmark and the startup/latency harness from S006): names start with the story ID.
- A story is done when its scenarios pass on all three CI platforms, nothing regressed, and the
  performance budgets still hold.

## Not in the MVP (from the brief, kept in mind)

Bold, italic, inline code, links, task and numbered lists, blockquotes, code blocks, tables,
images, search, spellcheck, export, wiki links, multiple documents, plugins, keybinding schemes,
localisation. The element registry (S002/S003) and theme tokens (S005) are designed so these can
be added without changing existing elements.
