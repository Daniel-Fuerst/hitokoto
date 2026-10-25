# S006 — Cold start within budget, measured in CI

- **Status:** Done in v0.1 · **Order:** 7 · **Depends on:** S003

**Implementation note:** Done (v0.1, 2026-10-24). Baselines in `bench/baselines/` come from CI run #18 (pull request #5, 2026-10-25). Idle CPU: no timers run while idle (the styler's idle timer stops when done); not yet measured automatically.

## Story

As a writer who opens hitokoto to jot something down, I want a usable, focused window almost
instantly, so that the editor never makes me wait.

## Acceptance scenarios

**Scenario: startup budget**
- Given a release build and a 10 KB Markdown file
- When I start `hitokoto file.md`
- Then the first painted frame with the focused editor appears in under 1000 ms on the reference
  machines (target ~500 ms, hard limit 1.5 s)

**Scenario: regression gate in CI**
- Given the startup benchmark on each CI platform
- When a change makes the median startup more than 20 % slower than the stored baseline
- Then the CI job fails with the measured numbers

**Scenario: idle**
- Given the app is open and nothing happens
- Then its CPU use over 10 s stays at 0 % (no timers running except the caret blink, which
  stops after inactivity)

## Tests

- `bench/S006_startup` – harness: an env flag makes the app print "exec → first frame" and quit
  (as in the Phase 0 spike); runs 10 times, reports median/p95.
- `bench/S006_typing` – key-to-paint latency on a small file (input for later stories).

## Notes

- CI runners are noisy; the gate compares against the platform's own baseline, not the absolute
  budget. The absolute budget is checked on real machines in S011.
