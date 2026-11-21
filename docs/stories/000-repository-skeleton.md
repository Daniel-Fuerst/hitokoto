# S000 — Repository skeleton, build and CI on three OSes

- **Status:** Done in v0.1 · **Order:** 1 · **Depends on:** –

**Implementation note:** Done (v0.1, 2026-10-24). CMake presets, core/ui/app layers with configure-time Qt guard, GoogleTest + Qt Test, clang-format/clang-tidy, ASan/UBSan and fuzz jobs, GitHub Actions matrix (Linux Qt 6.12 and 6.8, Windows, macOS). The CI workflow has not run yet; first run on GitHub pending.

## Story

As a developer, I want a buildable, tested skeleton on Linux, Windows and macOS, so that every
later story starts from green CI and enforced standards.

## Acceptance scenarios

**Scenario: the app builds and starts everywhere**
- Given a fresh clone and the documented prerequisites
- When I configure and build with CMake presets (`cmake --preset dev`, `cmake --build --preset dev`)
- Then `hitokoto` builds on Linux, Windows and macOS and opens an empty, focused editor window

**Scenario: layering is enforced**
- Given the targets `hitokoto_core` (C++20 standard library only) and `hitokoto_ui` (Qt)
- When `core` includes any Qt header or links any Qt target
- Then the build fails

**Scenario: CI guards quality**
- Given a pull request
- When CI runs
- Then it builds and runs core and UI tests on `ubuntu`, `windows` and `macos` runners, checks
  clang-format, runs clang-tidy, and runs the core tests under ASan/UBSan on Linux
- And the UI tests run headless (`QT_QPA_PLATFORM=offscreen`)

## Tests

- `TEST(S000_Skeleton, CoreLinksWithoutQt)` – trivial core function, proves the core test target.
- `tst_s000_skeleton` – starts the main window offscreen, asserts the editor widget has focus.
- CMake check that `hitokoto_core` has no Qt in its link interface.
- Added with the rework of 2026-11-21: `tst_s000_random_edits` (random edits with fixed seeds, see
  below); the workflow lints `S000_EveryActionIsPinnedToACommit`,
  `S000_AqtinstallIsPinnedToOneVersion`, `S000_EveryWorkflowStartsWithoutPermissions`,
  `S000_EveryCiJobAsksForItsPermissions`, `S000_OnlyTheStepThatCallsGhHasTheToken`,
  `S000_TheUiTestsRunWithSanitizers`, `S000_TheFuzzJobRunsEveryTarget`,
  `S000_PartOfTheFuzzTimeLimitsEachInput` and `S000_TheFuzzCorpusIsKept`
  (`tests/workflows/s000_workflow_cases.cmake`).

## Notes

- Layout: `src/core`, `src/ui`, `src/app`, `tests/core`, `tests/ui`, `bench`, `docs`.
- Dependencies: Qt 6 (Core, Gui, Widgets, Test); GoogleTest and Google Benchmark pinned via
  CMake `FetchContent` (tests and benchmarks only). Nothing else.
- Qt in CI: distribution packages on Linux where new enough, otherwise a pinned Qt installer
  action; record the choice in `docs/architecture.md`.
- `.gitattributes`: text files normalised, but test fixtures (`tests/fixtures/**`) marked
  `-text` so CRLF/BOM fixtures stay byte-exact.
- Creates `README.md` (build/run per OS) and `docs/architecture.md` (layers, interfaces).

## Rework on 2026-11-21

Branch `agents/s000-pinned-actions-and-build-hardening`, from main c8a167f.

- Actions come from a fixed commit. Every `uses:` of an action in the workflows names the commit
  its release tag pointed to on 2026-11-21, with the release as a comment
  (`actions/checkout` v7.0.1, `actions/upload-artifact` v7.0.2, `actions/download-artifact` v8.0.2,
  `jurplel/install-qt-action` v4.5.0, `actions/cache` v6.1.0). A tag can be moved to other code by
  whoever controls the action's repository, a commit cannot. Every call of install-qt-action names
  one release of aqtinstall (`aqtversion: "==3.3.0"`); on Windows `aqtsource`, the fixed commit of
  ADR 0013, still wins. To update an action, take the commit of its new release tag
  (`git ls-remote --tags`) and change commit and comment together. With every action pinned, the
  repository setting that requires full commits can be turned on.
- No job has a right it does not ask for. `ci.yml` starts with `permissions: {}` like the other two
  workflows, and every job of it asks for its own (`contents: read`, more only for `bench` and
  `version`). The token of `bench` reaches only the step that reads the labels with `gh`, not every
  step of the job.
- The CI checks what the suite alone does not:
  - Job "UI with ASan/UBSan (Linux)" (`ui-sanitizers`): the whole suite, the UI tests included,
    built with `HITOKOTO_SANITIZE=address,undefined` (RelWithDebInfo, Qt `QT_VERSION`, offscreen).
    The UI tests get longer time limits in this build (900 s instead of 300 s), and the limits inside
    them that a slower build can miss scale with `test::kTimeScale` (4 there, 1 otherwise); the whole
    suite took about 6 min on the development VM. The version job needs it (S035).
  - `tst_s000_random_edits`: six fixed seeds of 600 steps that mix typing, keys, the caret, undo
    and redo, pastes of text and HTML tables, find while the document changes, the source view of a
    table, resizing, scrolling and the theme; afterwards the document saves back byte for byte. The
    stories test these one at a time; their combinations meet here. About 25 s, also under the
    sanitizers. A failing seed is rerun alone with
    `tst_s000_random_edits randomEditsKeepTheEditorWhole:<seed>`.
  - Fuzz targets `fuzz_find` (S028) and `fuzz_table` (S018) beside the five before. The job names
    its targets once (`FUZZERS`, a lint keeps them equal to `fuzz/CMakeLists.txt`) and runs each for
    60 s: 45 s with libFuzzer's defaults, 15 s with inputs up to 256 KiB and at most 10 s per input,
    so that cost growing with the length of an input fails the run. The corpus of each target is
    kept between runs (`actions/cache`, keyed by run; a run restores the newest corpus of its branch
    or of main) and merged down to the inputs that add coverage; an input that fails is uploaded.
- The job "Format" hands the file list to clang-format through `xargs -0`, so no name is split at a
  space (actionlint reports nothing).
- A new job name must be added to the required checks of the ruleset of main, or every pull
  request waits for a check that never comes: here "UI with ASan/UBSan (Linux)".
- `SECURITY.md` says how to report a vulnerability privately (GitHub's "Report a vulnerability")
  and that fixes go into the newest release; the README points to it.
