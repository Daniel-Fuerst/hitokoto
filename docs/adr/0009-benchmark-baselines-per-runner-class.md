# ADR 0009: Benchmark baselines per runner class, recorded by CI, with main as the arbiter

- **Status:** Accepted (Armin, 2026-11-08, with the confirmation of S029; proposed with it)
- **Date:** 2026-11-08
- **Deciders:** project owner (Armin, Daniel)

## Context

The brief asks for benchmarks in CI that fail or flag the build on significant regressions
(section 4). Since S006 the Linux job compares `hitokoto-bench` with `bench/baselines/linux.json`,
one CI run per key, with a tolerance of 25 %. The runners behind `ubuntu-24.04` are at least four
kinds of hardware whose speed differs by more than that, and not by one factor (S029,
"Background"); the job fails without a code change and a restart is green. Normalising by runner
speed was declined on 2026-10-28. The Windows and macOS benchmarks are off since 2026-10-28.

Two questions are decided here: how the job decides that a pull request is slower, and where the
data for that lives. A third is where the decision logic lives in the source tree.

## Options: how the job decides

1. **One baseline for all runners** (status quo). Fails on a slow runner, misses regressions on a
   fast one.
2. **One baseline, normalised by the run's slowdown.** Declined: no single factor fits.
3. **A baseline per runner class, maintained by hand** from artefacts. Needs a person after every
   new class and after every accepted slowdown, for each class separately, and the class of a run
   cannot be chosen, so the data for a class arrives at random.
4. **Main and the pull request measured in turn on the same runner, always.** Needs no stored data
   and works on any runner; adds a build and several measuring runs to every run (S029 needs four
   rounds to tell openings that jump between two levels from a regression).
5. **A baseline per runner class, recorded by CI from runs of `main`, with option 4 as the arbiter**
   when the class has too few samples or the pull request exceeds the class baseline.

## Options: where the data lives

- **Commits to `main` by CI:** every sample changes `main`, starts CI again and races with merges;
  `main` changes need Armin's approval. Rejected.
- **Artefacts or the Actions cache:** artefacts expire and are scattered over runs, the cache is
  evicted after a week without use; neither is versioned or easy to read. Rejected.
- **A separate repository:** needs a token with write access stored as a secret and a second
  repository to look after. Rejected for now.
- **An orphan branch `bench-data` in this repository:** versioned, readable on GitHub, written with
  the workflow's own token, never merged, and pushes to it start no CI run (`ci.yml` runs on pushes
  to `main` only).

## Options: where the logic lives

- **In the workflow as shell or a script:** untested, a second language for real logic. Rejected.
- **In `hitokoto-bench` with Qt:** testable only through the executable.
- **In `src/core`:** follows the letter of CLAUDE.md but puts tooling into the product library that
  the brief defines as the editor's model, parsing, rendering, themes, I/O and settings.
- **A small module beside the benchmarks, standard library only:** the same rules as `src/core`
  (no Qt, unit-tested, built and tested in the Qt-free sanitizer job), but not part of the product.

## Decision

Option 5, with the data on the orphan branch `bench-data`, and the logic in a standard-library
module `bench/gate/` (library `hitokoto_bench_gate`) with its tests in `tests/bench/`, built
whenever tests are built, also in `core-sanitize`.

- The runner class comes from the `model name` of `/proc/cpuinfo`, the CPU count and the L2 and L3
  cache sizes; the clock is recorded for information. A class is established at 3 samples; its
  baseline is the median per key over its newest 5 samples. The main stage measures four fresh
  rounds (base, pull request, pull request, base) of the merge commit's first parent and fails a key
  only if the means differ beyond tolerance and the rounds of both sides do not overlap (S029,
  "Rules").
- Only runs of `main` (push, nightly schedule, `workflow_dispatch` on `main`) add samples; pull
  request runs have no write access. A deliberate slowdown is accepted by the label
  `bench-accepted`; hard budgets hold always.
- `hitokoto-bench` keeps measuring; a small executable `hitokoto-bench-gate` (gate library plus
  `Qt6::Core` for files and JSON) decides from files with the exit codes 0, 1 and 3 (green, fails,
  run the main stage), tested by `ctest` with fixed files; the workflow orchestrates only.
- Each class has an anchor (median of its oldest 3 samples after the newest acceptance marker); a
  run of `main` whose class baseline drifted beyond tolerance from it fails. Markers come from
  merges of pull requests with `bench-accepted` and from a `workflow_dispatch` input on `main` (spec
  review O2, Armin 2026-11-08).
- Only pull request runs are cancelled by newer runs on the same ref; runs of `main` finish, so
  every merge leaves its sample.

## Consequences

- Red Linux runs mean a pull request slower than main on the same machine, or a missed hard budget.
- Until the classes are established, and whenever a pull request exceeds its class baseline, the job
  builds the base commit and measures four more rounds: longer runs in exactly the cases that are
  in doubt.
- Slow drift over many merges stays visible: it turns `main` red until its cause is found or it is
  accepted, as a baseline change by hand did before (`bench/baselines/README.md`: "only on
  purpose").
- After an accepted slowdown, class baselines follow `main` as new samples come in; until then the
  arbiter keeps pull requests on those classes green if they are as fast as main.
- The repository gains a branch that only CI writes and nobody merges (nothing enforces that; a
  ruleset could, later); deleting it resets all
  classes to "not established", which is safe (the arbiter decides).
- `bench/baselines/linux.json` goes away; the CI workflow gets write access for one job on `main`.
- The same scheme fits Windows and macOS later (class from the CPU name of the platform), without
  changing the decision logic.

## Reconsider if

- The arbiter runs in most pull requests even after the classes are established (the class stage
  then saves little, and option 4 alone is simpler).
- Two runs of one commit on one runner differ by more than the tolerance (then neither stage can
  decide, and the measurements themselves need work).
- GitHub changes the runner hardware so often that classes never reach 3 samples.
- Writing to `bench-data` with the workflow token is restricted by repository or organisation
  settings, or the branch grows beyond what a fetch should take (then prune old samples or move to
  a separate repository).
