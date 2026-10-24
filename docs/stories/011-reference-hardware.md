# S011 — Budgets confirmed on reference hardware

- **Status:** Proposed · **Order:** 12 · **Depends on:** S007

## Story

As the project owner, I want the performance budgets checked on modest, typical machines, so
that "feels instant" is proven where users are, not only on CI runners.

## Acceptance scenarios

**Scenario: reference machines defined**
- Given `docs/performance.md`
- Then it lists 2–3 reference machines (one per OS, at least one older or low-power laptop) with
  CPU, RAM, disk, GPU, OS version

**Scenario: budgets measured**
- When the benchmark suite (S006, S007) runs on each reference machine, including a cold-cache
  start after reboot
- Then all budgets of the brief, section 4, are met, or each miss has an issue with analysis

## Tests

- Manual run of the existing benchmarks; results table with date and commit in `docs/performance.md`.

## Notes

- Repeat before each release.
