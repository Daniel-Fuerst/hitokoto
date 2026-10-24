# S000 — Repository skeleton, build and CI on three OSes

- **Status:** Proposed · **Order:** 1 · **Depends on:** –

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

## Notes

- Layout: `src/core`, `src/ui`, `src/app`, `tests/core`, `tests/ui`, `bench`, `docs`.
- Dependencies: Qt 6 (Core, Gui, Widgets, Test); GoogleTest and Google Benchmark pinned via
  CMake `FetchContent` (tests and benchmarks only). Nothing else.
- Qt in CI: distribution packages on Linux where new enough, otherwise a pinned Qt installer
  action; record the choice in `docs/architecture.md`.
- `.gitattributes`: text files normalised, but test fixtures (`tests/fixtures/**`) marked
  `-text` so CRLF/BOM fixtures stay byte-exact.
- Creates `README.md` (build/run per OS) and `docs/architecture.md` (layers, interfaces).
