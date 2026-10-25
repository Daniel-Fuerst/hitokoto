# Working agreement for coding agents

Read before any work: `docs/brief.md` (the full brief), `docs/adr/` (accepted decisions),
`docs/stories/README.md` (backlog and order).

- Stack (ADR 0001): C++20, Qt 6 Widgets, CMake. No QML, no JavaScript, no web technology.
- `src/core` uses the C++ standard library only, never Qt. All logic lives there and is unit-tested.
- One branch per feature, in backlog order. A branch may hold several closely related stories;
  anything beyond that only after asking. S009 is a standing quality gate outside the order.
- Only implement stories the user has confirmed. Confirmation may happen in conversation; it need
  not show in the commits. Test first (red, green, refactor); tests carry the story ID.
- Scope changes become new stories in `docs/stories/` with their reason; `docs/brief.md` stays the
  user's brief.
- A story is ready for review when its scenarios pass on Linux, Windows and macOS CI and the
  performance budgets still hold: set it to "In review" and summarise what changed, which tests
  were added, and what is next. It is done only when the user accepts the functionality; the
  user marks it "Done".
- Ask before decisions that are expensive to reverse or conflict with the brief; record them as
  ADRs in `docs/adr/`.
- New dependencies need a justification against the brief, section 3.

Build and check (see README.md for prerequisites per OS):

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
```

Architecture and interfaces: `docs/architecture.md`. Performance tools and numbers:
`docs/performance.md`. Theme format: `docs/theming.md` (a test keeps it complete).
