# MVP story backlog

- **Status:** Confirmed 2026-10-24; v0.1 implements S000–S008 and S010 (S007 partly)
- **Date:** 2026-10-24
- **Basis:** `docs/brief.md`, `docs/adr/0001-technology-stack.md` (C++20, Qt 6 Widgets, CMake)

Story IDs S001–S007 keep the numbers from the brief; S000 and S008–S012 are new. The order below
is the proposed implementation order. Each feature is one branch/PR and may hold several closely
related stories (more only after asking); stories are implemented test-first. Tests reference the
story ID (see "Conventions").

| Order | ID | Story | Depends on | Size | Status |
|---|---|---|---|---|---|
| 1 | [S000](000-repository-skeleton.md) | Repository skeleton, build and CI on three OSes | – | M | Done |
| 2 | [S001](001-open-file.md) | Opening a Markdown file shows its content | S000 | M | Done |
| 3 | [S004](004-save-byte-exact.md) | Save and Save As write the text byte for byte, atomically | S001 | M | Done |
| 4 | [S008](008-unsaved-changes.md) | Unsaved changes are never lost silently | S004 | S | Done |
| 5 | [S002](002-headings.md) | Headings render large, `#` hidden unless the cursor is on the line | S001 | L | Done |
| 6 | [S003](003-bullets.md) | Bullets render as dots, through the same element mechanism | S002 | M | Done |
| 7 | [S006](006-cold-start-budget.md) | Cold start within budget, measured in CI | S003 | M | Done (baselines from CI pending) |
| 8 | [S009](009-concealment-quality-gate.md) | Concealment quality gate: caret, IME, accessibility | S003 | M | Standing gate (outside the order) |
| 9 | [S010](010-click-on-concealed-line.md) | Clicking a line with hidden markers puts the caret where clicked | S009 | S | Done |
| 10 | [S005](005-theme-files.md) | A theme file changes the look; light and dark built in | S003 | L | Done |
| 11 | [S007](007-large-files.md) | 10 MB files open within budget and type without lag | S006 | L | Partly done |
| 12 | [S011](011-reference-hardware.md) | Budgets confirmed on reference hardware | S007 | S | Open |
| 13 | [S012](012-distributable-builds.md) | Distributable builds for Linux, Windows and macOS | S000 | M | Open |
| 14 | [S013](013-inline-emphasis.md) | Inline bold, italic and strikethrough | S003 | M | In progress (CI pending) |
| 15 | [S014](014-fenced-code-blocks.md) | Fenced Markdown code blocks | S013 | M | In progress (CI pending) |
| 16 | [S015](015-inline-code.md) | Inline Markdown code spans | S014 | S | In progress (CI pending) |
| 17 | [S016](016-application-icon.md) | Show the application icon in the taskbar or dock | S000 | S | In progress |

Sizes: S = up to a day, M = a few days, L = about a week of part-time work (rough).

Status: **Open** → **Confirmed** (by the user) → **In progress** → **In review** (CI green on all
three platforms, budgets hold, theming baseline met) → **Done** (the user has accepted the functionality; only the user
sets it).

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

- Story files: `docs/stories/NNN-title.md`, written before implementation and confirmed by the
  user. The confirmation may happen in conversation; it need not show in the commits.
- Scope changes: `docs/brief.md` stays the user's brief. Extensions become new stories here, with
  their reason. (The additions made to the brief together with S013–S015 stay for now.)
- S009 is a standing quality gate, not a one-off story: its scenarios are extended whenever an
  element conceals syntax, and it may leave the order.
- Core tests (GoogleTest): test suite named after the story, e.g. `TEST(S002_Headings, MarkerHiddenWhenCursorElsewhere)`.
- UI tests (Qt Test, `QT_QPA_PLATFORM=offscreen`): one test class per story, e.g. `tst_s002_headings`.
- Benchmarks (Google Benchmark and the startup/latency harness from S006): names start with the story ID.
- Theming baseline (brief, section 8): every element a story adds or changes has its own theme
  tokens for every visible property, documented in `docs/theming.md`. This applies to every story;
  it is not a story of its own.
- A story is ready for review when its scenarios pass on all three CI platforms, nothing
  regressed, the performance budgets still hold, and the theming baseline is met. It is done when the user has accepted it.

## Not in the MVP (from the brief, kept in mind)

Links, task and numbered lists, blockquotes, tables, images, search,
spellcheck, export, wiki links, multiple documents, plugins, keybinding schemes, localisation.
The element registry (S002/S003) and theme tokens (S005) are designed so these can be added
without changing existing elements.
