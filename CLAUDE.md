# Working agreement for coding agents

Read before any work: `docs/brief.md` (the full brief), `docs/adr/` (accepted decisions),
`docs/stories/README.md` (backlog and order).

- Stack (ADR 0001): C++20, Qt 6 Widgets, CMake. No QML, no JavaScript, no web technology.
- `src/core` uses the C++ standard library only, never Qt. All logic lives there and is unit-tested.
- Work one story at a time, in backlog order, on its own branch. Only implement stories whose
  status is confirmed. Test first (red, green, refactor); tests carry the story ID.
- A story is done when its scenarios pass on Linux, Windows and macOS CI and the performance
  budgets still hold. Afterwards summarise what changed, which tests were added, and what is next.
- Ask before decisions that are expensive to reverse or conflict with the brief; record them as
  ADRs in `docs/adr/`.
- New dependencies need a justification against the brief, section 3.
