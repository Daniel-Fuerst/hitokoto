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

## Benchmark job on Linux red without a code change

- **Seen:** repeatedly since S017, most recently on pull request #24 (2026-10-31): the Linux job
  fails the 25 % comparison and a restart is green. The first run of #24 had a fast runner (the
  older keys 10–45 % below their baseline); the `s024_*` values taken from it failed on the next,
  ordinary runner (`s024_leave_5000_keys_built_ms` 63 → 84 ms, `s024_leave_long_text_built_ms`
  37 → 55 ms), while the older keys lay about on their baseline again. Armin restarted the job
  three times; the artefacts show the same runner type each time (core decoding 20 ms at
  3.25 GHz, against 12 ms at 3.7 GHz in the first run), and the three runs agree within a few per
  cent, except `s024_open_10mb_properties_ms` (31, 55, 61 ms) and single maxima
  (`typing_small_max_ms` 6.8 ms once). `open_10mb_styled_ms` lies 19–20 % above its baseline in
  all three (3.08–3.12 s against 2.60 s) but only 2 % above it on the fast runner, so it depends
  on more than CPU speed (idle slices, timers); it is 5 % from failing.
- **How the gate works:** `bench/baselines/linux.json` holds the values of one CI run per key; new
  keys come from the first run of the pull request that adds them. A key fails when it is 25 %
  and 2 ms above its baseline; medians and p95/p99 are compared, maxima and counts only reported.
  Runner speed varies from run to run, and not by one factor for all kinds of work (2026-10-28:
  decoding ×2.0, rendering ×1.04, typing ×1.8), so Armin declined normalising by runner speed.
- **Options (open):** (1) a baseline per key from several runs (the slowest, or a high
  percentile), and new keys only reported until they have it; (2) main and the branch measured
  in turn in the same job on the same runner, the branch compared with main (as done locally;
  about twice the job time and a change to `ci.yml`); (3) completion times in idle slices only
  reported, as `s023_bullet_settled_ms` and `s023_underline_settled_ms`, with the hard 50 ms
  budgets of the keys kept; (4) the first sample of an opening measurement dropped as warm-up
  (`s024_open_10mb_properties_ms`: 49 ms, then 31 ms); (5) the runner's core decoding time,
  already in the artefact, printed beside the comparison, so that a red run on a fast or slow
  runner is recognised at once; (6) **preferred for now (Armin, 2026-10-31):** a baseline per class
  of runner. The job records the runner's CPU (model name from `/proc/cpuinfo`, clock; the core
  decoding time as a cross-check) and compares against the baseline of that class, for example
  `bench/baselines/linux-<cpu>.json`; no factor converts between classes. A runner of a class
  without a baseline fails the job with the message that a new class has to be recorded, and
  leaves its values as an artefact to start that baseline. Within a class the spread is small (a
  few per cent in the three runs above), so the tolerance could be tighter than 25 %. First step:
  record the CPU in the artefact and collect a few runs to learn the classes (so far four, see
  below, and once core decoding of 24 ms on 7e9e7d4).
- **Classes of runner seen on pull request #24 (2026-10-31):** seven runs, all `ubuntu-24.04` with
  4 vCPU and the host name `runnervm48cm3` (the host name does not tell the classes apart). Classes
  by `context.mhz_per_cpu` in `core-bench.json`; core decoding and rendering are
  `S007_Decode10MB` and `S007_RenderAllLines10MB`, the other columns are keys of
  `e2e-bench.json`:

  | Class | Clock | Runs | Decoding | Rendering | `open_10mb_styled` | `s023_open_10mb_styled` | `typing_10mb_median` |
  |---|---|---|---|---|---|---|---|
  | A | 3694 MHz | 1st, 7th | 12.2–12.4 ms | 44.6–44.7 ms | 2.63–2.64 s | 2.26 s | 0.71 ms |
  | B | 3244–3265 MHz | 2nd–4th | 19.7–20.4 ms | 60.5–60.7 ms | 3.08–3.12 s | 2.72–2.76 s | 0.91–0.94 ms |
  | C | 2800 MHz | 5th | 23.1 ms | 49.3 ms | 3.34 s | 2.90 s | 1.29 ms |
  | D | 4540 MHz | 6th | 7.7 ms | 30.7 ms | 1.89 s | 1.60 s | 0.53 ms |

  Within a class the runs agree within a few per cent; between classes there is no common factor
  (C against B: decoding ×1.17, rendering ×0.81, typing ×1.4). The baseline in use
  (`open_10mb_styled_ms` 2.60 s, `typing_10mb_median_ms` 0.90 ms) matches no class completely.
  Class C failed `open_10mb_styled_ms` (+28 %), while the same commit lay within the noise of main
  locally (+2 %). Class D passed every comparison but failed the hard budget: one of 200 keys in
  `typing_small` took 54.2 ms (`typing_small_max_ms`; p99 0.63 ms). `typing_small` is the first
  typing measurement after the window opens; pull request #20 showed the same once (222 ms, a
  restart was green), so it is taken as a hiccup of the runner, not a property of the class. The
  artefact does not yet name the CPU model, and it shows a maximum without the key that caused it;
  with (6), print samples above 50 ms with their position. Armin keeps the artefacts
  (`bench-linux-PR24.zip` and `-1` to `-6`) outside the repository.
- **Open:** which of these to adopt, (6) preferred. Armin's goal: the job is red only for a real
  regression, and a new kind of runner is never overlooked.

## Versioning, and packages and installers for every version

- **Seen:** 2026-10-31 (Armin, after building main for daily use): every update means building
  locally, copying the exe and running `windeployqt` by hand.
- **Now:** the version is `0.1.0` in `project()` in `CMakeLists.txt`, set by hand and unchanged
  since S000; the app reports it through `QApplication::applicationVersion`. The repository has
  no tags. `CMakeLists.txt` configures CPack for DEB and TGZ (pull request #15), but no CI job runs
  it. S012 (distributable builds; Open in the backlog, Proposed in its file) asks for one artefact
  per platform from a release workflow for a tagged release: a Windows installer or zip, a macOS
  `.dmg` and a Linux AppImage, with the licences.
- **Wanted (Armin):** a version, set automatically if possible, and for every new version packages
  and installers built automatically.
- **Open:** where the version comes from (a git tag read by CMake, a file, or derived from the
  commit messages); what makes a new version (a tag on main, a GitHub release, every merge); the
  formats per platform and whether this refines S012 or becomes a story of its own; signing
  (Windows certificate, macOS notarisation); where the artefacts go (GitHub releases, now that the
  repository is public). A release job changes `ci.yml` or adds a workflow, so it needs Armin's
  approval.
