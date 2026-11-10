# Not yet refined

Points noticed in use or during an implementation that are not specified yet, including known
failures whose cause or remedy is still open. An entry says what was seen, what CommonMark (or GFM,
where hitokoto follows it) says, and what is open. When an entry is refined, it becomes a story in
this folder and is removed here, with the story ID in the commit message. This file may be updated
on any branch; it needs no branch of its own (Armin, 2026-10-30). Expectations are checked against
commonmark.js or markdown-it-py (mode `commonmark`) before they are written here.

## Emphasis across a line break

- **Seen:** 2026-11-04 (Armin, Windows release on main 6c67e67, a bullet list with hard-wrapped
  items; confirmed as a bug by Daniel). `- … (manuell, VPN), **nie durch einen` followed by
  `  Agenten**. Kontrolle …` shows both `**` as text and nothing bold; Obsidian shows "nie durch
  einen Agenten" bold.
- **CommonMark:** inline content belongs to the whole paragraph, so delimiters pair across soft
  line breaks: `**a` / `b**` gives `<strong>a\nb</strong>`, also in a list item with or without
  the continuation indent (`- **a` / `  b**`, `- **a` / `b**`); `*a` / `b*` and `_a` / `b_` give
  `<em>`; `` `a `` / `` b` `` gives `<code>a b</code>`; `[a` / `b](u)` is one link. Nothing pairs
  across a block boundary: not over a blank line, not from one list item to the next (`- **a` /
  `- b**`), not out of a heading (`# **a` / `b**`), not into a fenced code block. GFM
  strikethrough pairs across the line break as well (`~~a` / `b~~` gives `<del>`). Checked with
  commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode `commonmark`), `~~` with cmark-gfm.
- **hitokoto:** inline elements are decided per line (S013: "Nested and adjacent combinations work
  within one line"; S015 and S022 work on one line as well); an unpaired delimiter stays text.
- **Open:** pairing over the lines of a paragraph (ADR 0006 gives the paragraph context and
  `renderDocument` the reference); which lines reveal their markers when the caret is on one of
  them; what an edit on one line re-renders on the others; the cost for long hard-wrapped
  paragraphs (S007 budgets). Related: soft line breaks above, bullet list indentation below.

## Indentation of bullet lists

- **Seen:** 2026-10-28 (Armin, Windows release on main e1d011e, `docs/stories/021-code-block-fences-in-panel.md`).
  Continuation lines of a hard-wrapped bullet start with two spaces; hitokoto shows those spaces as
  indentation, so the line does not stand under the item's text. Lines the window wraps start at the
  left edge instead of under the item's text. Seen again 2026-11-04 (Armin, main 6c67e67, after
  `fix/bullet_points`; confirmed as a bug by Daniel): hard-wrapped continuation lines
  (`  Agenten**.`, `  erfassen.`) still do not stand under the item's text; Obsidian indents them
  like the item's text.
- **CommonMark:** continuation lines indented to the item's content column (marker, then 1–4
  spaces) belong to the item's paragraph; unindented ones continue it lazily. Nesting is decided by
  the content column of the parent item, not by a fixed indentation per level.
- **hitokoto:** the nesting depth is the indentation divided by 2 (`bullet.cpp`, S003); continuation
  lines are plain lines.
- **Merged:** Daniel's `fix/bullet_points` (pull request #23, cdd2d21, 2026-11-03, scenarios added
  to S003) aligns window-wrapped continuation lines with the item's text (hanging indent),
  continues and leaves lists with Enter, indents with Tab and Shift+Tab, and adds a default indent.
- **Open:** hard-wrapped continuation lines (concealing their indentation, hanging indent); nesting
  by content column. Related: soft line breaks and emphasis across a line break above, numbered
  lists below.

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

## Packages and installers for every version

- **Seen:** 2026-10-31 (Armin, after building main for daily use): every update means building
  locally, copying the exe and running `windeployqt` by hand.
- **Refined so far:** the version itself is S030 (a defined version step for every merge into
  main, ADR 0010).
- **Now:** `CMakeLists.txt` configures CPack for DEB and TGZ (pull request #15), but no CI job runs
  it; it installs only the binary (no `.desktop` file, icon or copyright file). S012 (distributable
  builds; Open in the backlog, Proposed in its file) asks for one artefact per platform from a
  release workflow for a tagged release: a Windows installer or zip, a macOS `.dmg` and a Linux
  AppImage, with the licences.
- **Wanted (Armin):** for every new version, packages and installers built automatically; on
  2026-11-08 first of all a Debian package for trixie, built by CI on main against Debian's Qt
  (6.8.2), once versions exist.
- **Open:** the formats per platform and whether this refines S012 or becomes a story of its own
  (S012 still says the `.deb` follows separate Debian packaging conventions; for Armin, a `.deb`
  built by CI belongs to this project); signing (Windows certificate, macOS notarisation); where the
  artefacts go (GitHub releases, now that the repository is public). Tags pushed by the S030 tag
  job start no other workflow, so package builds run in the same workflow after it. A release job
  changes `ci.yml` or adds a workflow, so it needs Armin's approval. GitHub Releases with generated
  notes come with the packages, not with S030 (Armin, 2026-11-09), so that a release carries
  something to download.
- **Version of the packages (code review of S030, O6, 2026-11-09):** `CPACK_PACKAGE_VERSION` is the
  full version from S030 (`0.4.0+3.gabc1234`); DEB and TGZ take it. Windows installers (WiX, Inno
  Setup) and version resources need a numeric version: they take `HITOKOTO_VERSION_CORE`, not
  `CPACK_PACKAGE_VERSION`.

## Showing the version to the user

- **Seen:** 2026-11-09 (Armin, while refining S030): S030 gives every build a version, but the user
  sees it hardly anywhere.
- **Now:** the version goes to `QApplication::applicationVersion()` (S030). `hitokoto --version`
  shows it (the `QCommandLineParser` option exists since S000; on Windows a message box, because the
  program is a GUI application; found while implementing S030); there is no About dialog and no
  version in the title bar.
- **Open:** where else it is shown: a dialog Help › About (with the licence and the Qt version), the
  title bar, or both; theme tokens for a dialog (brief, section 8); whether builds between tags show
  the full version (`0.4.0+3.gabc1234`) or the numeric core.

## The check "Version label" with the script from main

- **Seen:** 2026-11-09 (code review of S030, O3): the job "Version label" in `version.yml` runs
  `scripts/version/NextVersion.cmake` from the pull request's own merge ref. A pull request that
  changes the script changes its own required check; a script that always succeeds would let a
  pull request without a version label through.
- **Now:** accepted for the time being (Armin, 2026-11-09). The tag job runs the script from main
  and stops after the merge at a pull request without exactly one version label (red, names the
  pull request).
- **Wanted (Armin):** follow-up work once S030 is on main: the check takes the script from main
  (the base of the pull request), not from the pull request. Before the merge of S030 main has no
  script, so it cannot be done in S030 itself.
- **Purpose (Armin, 2026-11-09):** a guard against an accidental change of the script, not
  against a deliberate one. GitHub runs `version.yml` for a pull request from the pull request
  itself, so a pull request can still change or remove the check there; that must stay possible,
  for example to move to another way of versioning (removing the check also takes it out of the
  required checks of the ruleset). Closing that too would need `pull_request_target`, which S030
  rules out.
- **Open:** whether the check checks out the base commit or main's tip, and how a pull request that
  changes the script itself is checked (with main's script, so that a change to the rules applies
  from the next pull request on).

## Benchmark baselines per runner class (deferred)

- **Seen:** 2026-11-09 (Armin, code review of S029). The first version of S029 compared the Linux
  benchmark job with a baseline per runner class, recorded by CI on a branch `bench-data`, and was
  implemented and verified in CI; it was replaced before the merge by measuring main on the same
  runner in every pull request run (ADR 0009, revision of 2026-11-09).
- **Now:** every pull request run builds the base and measures four rounds; the runner class is
  only reported. The full design of the class variant, with the dispositions of the code review
  that apply to it, is in ADR 0009, "Deferred".
- **Open:** whether it is needed. Candidates: the bench job's duration becomes a burden, or drift
  over many merges within the tolerance becomes a concern (the anchor of the class variant, or a
  comparison with the last release tag on the same runner). Window, minimum and tolerance would be
  tuned on recorded data.

## Tables with more than 255 columns

- **Seen:** 2026-11-09 (S031, while implementing). S018 recognises at most 255 columns
  (`kMaxColumns` in `src/core/src/elements/table.cpp`; the column count lives in 8 bits of the
  block state). A wider table, typed or pasted (S031 writes its Markdown without a size limit),
  is shown as text.
- **GFM:** no column limit; cmark-gfm (cmarkgfm 2025.10.22) and markdown-it-py 4.2.0 (tables on)
  both render a 300-column table as a table.
- **hitokoto:** up to 255 columns a table, above that every line is text.
- **Open:** whether wider tables matter in practice; if so, more bits for the column count in the
  block state (ADR 0006) and the cost of showing such a table (S018 budgets).
