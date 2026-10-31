# Not yet refined

Points noticed in use or during an implementation that are not specified yet, including known
failures whose cause or remedy is still open. An entry says what was seen, what CommonMark (or GFM,
where hitokoto follows it) says, and what is open. When an entry is refined, it becomes a story in
this folder and is removed here, with the story ID in the commit message. This file may be updated
on any branch; it needs no branch of its own (Armin, 2026-10-30). Expectations are checked against
commonmark.js or markdown-it-py (mode `commonmark`) before they are written here.

## Soft line breaks: paragraphs flow

- **Seen:** 2026-10-30 (Armin, `docs/adr/0006-context-beyond-one-line.md` on Windows). A paragraph
  that is hard-wrapped in the source shows each source line as its own line; a source line wider
  than the window wraps, so its last word can stand alone, and the next source line starts anew.
- **CommonMark:** a line ending inside a paragraph is a soft line break and renders as a space; the
  paragraph flows (`a` / `b` gives `<p>a\nb</p>`). A hard line break needs two trailing spaces or a
  backslash before the line ending (`a  ` / `b` gives `a<br />b`).
- **hitokoto:** every source line is a line on screen (S001–S003); a soft break is not concealed.
- **Open:** whether paragraphs flow as in the rendered document while the caret is elsewhere, and
  what happens on the caret line (show the source lines again, like revealed markers); how a hard
  line break is shown; how the caret moves over a flowing paragraph; the cost for long hard-wrapped
  paragraphs (S007 budgets). Related: bullet list indentation below, ADR 0003 (editor surface).

## Indentation of bullet lists

- **Seen:** 2026-10-28 (Armin, Windows release on main e1d011e, `docs/stories/021-code-block-fences-in-panel.md`).
  Continuation lines of a hard-wrapped bullet start with two spaces; hitokoto shows those spaces as
  indentation, so the line does not stand under the item's text. Lines the window wraps start at the
  left edge instead of under the item's text.
- **CommonMark:** continuation lines indented to the item's content column (marker, then 1–4
  spaces) belong to the item's paragraph; unindented ones continue it lazily. Nesting is decided by
  the content column of the parent item, not by a fixed indentation per level.
- **hitokoto:** the nesting depth is the indentation divided by 2 (`bullet.cpp`, S003); continuation
  lines are plain lines.
- **In progress elsewhere:** Daniel's branch `fix/bullet_points` (6ad446d, e53cf0c, ee5bada,
  2026-10-30, not merged) aligns window-wrapped continuation lines with the item's text (hanging
  indent), continues and leaves lists with Enter, indents with Tab and Shift+Tab, and adds a default
  indent. It changes the behaviour of S003 (Done), so it needs a story of its own.
- **Open:** hard-wrapped continuation lines (concealing their indentation, hanging indent); nesting
  by content column; what is left after `fix/bullet_points`. Related: soft line breaks above,
  numbered lists below.

## Numbered lists

- **Seen:** listed as not in the MVP (brief, `README.md`); S023 treats them only as block starts.
- **CommonMark:** an ordered list item is 1–9 digits followed by `.` or `)` and a space (or the end
  of the line). The first item's number is the list's start (`3.` / `7.` gives `<ol start="3">`);
  later numbers do not matter. The content column is the marker's width plus 1–4 spaces, so
  continuation lines of `10. foo` are indented four spaces, those of `1. foo` three. Bullets and
  ordered lists nest in each other by the content column (`1. a` / `   - b`). Only a list starting
  with `1` can interrupt a paragraph; `Foo` / `2. x` is one paragraph.
- **hitokoto:** plain text.
- **Open:** how the marker looks while the caret is elsewhere (as written, or numbered from the
  start), its tokens; continuation lines and nesting with the correct indentation (shares its rules
  with bullet list indentation above); Enter and Tab like `fix/bullet_points` does for bullets.

## Blank lines as paragraph spacing

- **Seen:** 2026-10-27, during S018.
- **CommonMark:** a blank line only separates blocks; several blank lines render like one
  (`a`, three blank lines, `b` gives two paragraphs, no extra space). The space between paragraphs comes
  from the rendering.
- **hitokoto:** every blank line is a line of its own; S018 hides the one blank line below a table.
- **Open:** whether blank lines between paragraphs are concealed and replaced by paragraph spacing
  (a token), and what the caret does there. To decide with Daniel.

## tst_s017 fails locally on Windows

- **Seen:** 2026-10-28 (Armin, ttv-e0536, Visual Studio 2022, Qt 6.12.0 MSVC), again 2026-10-30 in the
  Test Explorer. CI (Release, Qt 6.8.3 on Windows) is green.
- **What fails:** in the Debug build (preset `win-dev`) the test runs into the ctest timeout of 300 s
  and reports a stall of 61 ms; in a Release build that inherits `ci` the test
  `savingTenMegabytesDoesNotStallTheEventLoop` reports a longest stall of 18 ms (best of three)
  against the bound `max(copyMs / 2, 16)` = 16 ms, with a one-shot copy of 16 ms; the other nine
  cases pass. On this fast machine (22 threads) the stall is longer than the whole copy, so it does
  not come from the slices.
- **Cause:** open. Guesses without evidence: the virus scanner closing the 10 MB file in the temp
  folder (outside the excluded `C:\Dev`), freeing large strings on the UI thread, Qt 6.12 instead of
  6.8.3. The 1 ms ticker is no artefact: Qt uses multimedia timers for coarse timers up to 20 ms on
  Windows (`qeventdispatcher_win.cpp`).
- **2026-10-31** (Armin, `win-dev`, Qt 6.12.0 MSVC debug build, on `agents/s024-front-matter`
  after the merge of main with S022): `savingTenMegabytesDoesNotStallTheEventLoop` reports a one-shot
  copy of 34 ms and a longest stall of 35 ms against the bound 17 ms. The cases up to
  `pendingSaveIsWrittenWhenAnotherFileIsOpened` pass; `quitSavesSynchronouslyWhileACopyRuns` prints
  no result before the ctest timeout of 300 s, so the timeout comes from that case (or from the
  time all cases take together in the debug build), not from the stall measurement. Whether it
  hangs or only runs slowly is open: a run of that case alone
  (`tst_s017_save_without_stall.exe quitSavesSynchronouslyWhileACopyRuns`) would tell.
- **Open:** find the cause on Windows; Armin's goal is a green `win-dev`, for example by skipping
  timing assertions in unoptimised builds (`QSKIP`), without weakening the gate in CI.

## IntelliSense errors in Qt headers in Visual Studio

- **Seen:** 2026-10-31 (Armin, `win-dev`, Visual Studio 2022 17.14 with MSVC 14.44, Qt 6.12.0 MSVC)
  after a full rebuild following the merge of S022: the Error List shows two IntelliSense entries
  for `tst_s005_theme.exe`, E2422 "defaulted default constructor cannot be constexpr because the
  corresponding implicitly declared default constructor would not be constexpr" in
  `QtCore/qrunnable.h` line 26, and E0028 "expression must have a constant value" in
  `QtCore/qobjectdefs.h` line 604. The compiler (`cl`) builds the same code without error or
  warning; the test target reaches `qrunnable.h` through `document_session.hpp` (since S017).
- **Gone again:** after Armin deleted the CMake cache and built again the same day, the entries
  did not come back; probably IntelliSense worked on a state from before the merge. Open whether
  they return.
- **Not a fix:** editing the Qt headers (Copilot proposed removing `constexpr` from `QRunnable()`
  and `QAtomicInt(int)`). The headers are correct for `cl`, a changed installation is not
  reproducible on other machines or in CI, and it would change what the real compiler sees.
- **Cause:** open. IntelliSense uses its own front end (EDG), which may assume another language
  standard or compiler mode than the build, for example `/Zc:__cplusplus`, or may not yet support
  what Qt 6.12 relies on there.
- **Open:** find the IntelliSense setting (CMake presets, `CMakeSettings`, `intelliSenseMode`) that
  makes the entries go away, or a Qt or Visual Studio bug report; until then the Error List filter
  "Build Only" hides them. Armin's goal: no entries in the Error List.
