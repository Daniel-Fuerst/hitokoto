# ADR 0009: Benchmarks compared with main on the same runner; baselines per runner class deferred

- **Status:** Accepted (revision of 2026-11-09, Armin, with the revised S029). The first version,
  "Benchmark baselines per runner class, recorded by CI, with main as the arbiter", was accepted on
  2026-11-08 with S029 and implemented on `agents/s029-bench-runner-classes`; the code review
  (`docs/reviews/029-bench-runner-classes-code.md`) led Armin to the simpler option 4 on
  2026-11-09, before anything was merged. The design of the first version is kept below under
  "Deferred".
- **Date:** 2026-11-08, revised 2026-11-09
- **Deciders:** project owner (Armin, Daniel)

## Context

The brief asks for benchmarks in CI that fail or flag the build on significant regressions
(section 4). Since S006 the Linux job compares `hitokoto-bench` with `bench/baselines/linux.json`,
one CI run per key, with a tolerance of 25 %. The runners behind `ubuntu-24.04` are at least four
kinds of hardware whose speed differs by more than that, and not by one factor (S029,
"Background"); the job fails without a code change and a restart is green. Normalising by runner
speed was declined on 2026-10-28. The Windows and macOS benchmarks are off since 2026-10-28.

A comparison with values from another machine is unreliable in both directions: it fails a pull
request on a slower runner, and it passes a regression on a faster one (decoding 10 MB takes 8 ms
on the fastest class and 24 ms on the slowest, so a pull request can double the time on a fast
runner and still stay below a baseline from a slow one). Only the first kind was ever noticed.

Three questions are decided here: how the job decides that a pull request is slower, how changes
of the toolchain are compared, and where the decision logic lives in the source tree.

## Options: how the job decides

1. **One baseline for all runners** (status quo). Fails on a slow runner, misses regressions on a
   fast one.
2. **One baseline, normalised by the run's slowdown.** Declined: no single factor fits.
3. **A baseline per runner class, maintained by hand** from artefacts. Needs a person after every
   new class and after every accepted slowdown, for each class separately, and the class of a run
   cannot be chosen, so the data for a class arrives at random.
4. **Main and the pull request measured in turn on the same runner, always.** Needs no stored data
   and works on any runner; adds a build and four measuring runs to every pull request run.
5. **A baseline per runner class, recorded by CI from runs of `main`, with option 4 as the arbiter**
   when the class has too few samples or the pull request exceeds the class baseline (the first
   version of this ADR; see "Deferred").
6. **One baseline as in option 1, with option 4 only when the job would fail.** Fixes the false
   failures but keeps the false passes: a green first comparison is never checked on the same
   runner. Declined (Armin, 2026-11-09).

## Options: the toolchain

The base is built from its own checkout (`git worktree`), so its `CMakePresets.json`, `cmake/`
and the versions of GoogleTest and Google Benchmark fetched there are the base's own; changes of
those are compared correctly by option 4. Shared by both sides are only what `ci.yml` installs on
the runner before the build, in practice the Qt version, and the runner image.

- **A notice only.** A slower Qt passes unseen: both sides are built with it.
- **A notice and a forced label.** Makes somebody compare by hand on every Qt update; the comparison
  itself stays manual.
- **Build the base with its own Qt.** When the Qt version of the bench job differs between the base
  and the pull request, the job also installs the base's Qt and builds the base with it. Compares a
  Qt update automatically, at the cost of one conditional step.

The runner image cannot be switched within a job; a change of it can only be detected and named.

## Options: where the logic lives

- **In the workflow as shell or a script:** untested, a second language for real logic. Rejected.
- **In `hitokoto-bench` with Qt:** testable only through the executable.
- **In `src/core`:** follows the letter of CLAUDE.md but puts tooling into the product library that
  the brief defines as the editor's model, parsing, rendering, themes, I/O and settings.
- **A small module beside the benchmarks, standard library only:** the same rules as `src/core`
  (no Qt, unit-tested, built and tested in the Qt-free sanitizer job), but not part of the product.

## Decision

Option 4, the base built with its own Qt when the Qt version differs, and the logic in a
standard-library module `bench/gate/` (library `hitokoto_bench_gate`) with its tests in
`tests/bench/`, built whenever tests are built, also in `core-sanitize` (Armin, 2026-11-09).

- Every pull request run builds the base (the first parent of the merge commit CI tests) with the
  `bench` preset in a separate directory and measures four rounds, base, pull request, pull request,
  base, on the same runner. A compared key fails only if the means of the two sides differ beyond
  the tolerance and the rounds of both sides do not overlap (S029, "Rules").
- A *toolchain change* is a change of the Qt version or the runner image (`os`) of the bench job in
  `ci.yml`, of `CMakePresets.json` or of anything under `cmake/`. A pull request with a toolchain
  change may change nothing else except `docs/` and `*.md`; otherwise the job fails. A change of the
  runner image is named in the report as not visible to the comparison.
- Runs of `main` measure once and check the hard budgets; changes reach `main` through pull
  requests, which are compared.
- A deliberate slowdown is accepted by the label `bench-accepted`; hard budgets hold always.
- `hitokoto-bench` keeps measuring; a small executable `hitokoto-bench-gate` (gate library plus
  `Qt6::Core` for files and JSON) decides from files with the exit codes 0 and 1 (green, fails),
  tested by `ctest` with fixed files; the workflow orchestrates only. The runner class is still
  derived and reported, for information and as data for "Deferred".

## Consequences

- Red Linux runs mean a pull request slower than main on the same machine, or a missed hard budget,
  or a toolchain change mixed with other changes. Green means not slower than main on the same
  machine.
- Every pull request run takes the base build and four rounds: about 8 instead of about 3 minutes
  for the bench job (7 min 52 s measured on pull request #31).
- Nothing is stored between runs: no branch that CI writes, no job with write access, no nightly
  run. `bench/baselines/linux.json` goes away.
- Slow drift over many merges, each within the tolerance, is not detected; only the hard budgets
  bound it.
- A slowdown caused by a new runner image is not detected; it affects base and pull request alike.
- The same scheme fits Windows and macOS later without changing the decision logic.

## Reconsider if

- The bench job's duration becomes a burden for pull requests (then the class stage of "Deferred"
  saves the base build in most runs).
- Drift over many merges becomes a concern, or a release turns out noticeably slower than the one
  before without a single red run (then the anchor of "Deferred", or a comparison with the last
  release tag on the same runner).
- Two runs of one commit on one runner differ by more than the tolerance (then the measurements
  themselves need work).

## Deferred: baselines per runner class (first version, 2026-11-08)

Implemented on `agents/s029-bench-runner-classes` up to ab76b11 and verified in CI (pull request
run #121, dispatches 42532362126 and 42575960727), then replaced by option 4 before the merge. If
it comes back, it adds a class stage in front of option 4 and is built as follows.

- **Runner class:** `linux-<model>-<n>cpu-l2-<L2>-l3-<L3>` from the first `model name` of
  `/proc/cpuinfo` (lower case, runs of other characters as one `-`), `nproc` and the L2 and L3
  sizes from `/sys/devices/system/cpu/cpu0/cache/index2` and `index3`. Without a model name the
  class is `linux-unknown-…`: it gets no samples, no anchor and no drift check and always compares
  with main (review O10).
- **Samples:** only runs of `main` (push, nightly schedule, `workflow_dispatch` on `main`) add one,
  validated as complete (every hard-budget key and the context fields), as
  `linux/<class>/<start time>-<short commit>-<run ID>-<attempt>.json` on an orphan branch
  `bench-data`, written by a separate job with `contents: write` that retries a rejected push. A
  separate repository, artefacts, the Actions cache and commits to `main` were rejected as storage.
- **Class stage:** a class is established at 3 readable samples; its baseline is the median per key
  over the newest 5 samples that contain the key. Green if every compared key is within tolerance;
  otherwise, or if the class is not established, option 4 decides. The report shows the window's
  minimum and maximum besides the median (review O1). Runs of `main` report the class stage as
  information, never as `SLOW` (review O12).
- **Anchor and drift:** an acceptance marker is written for the merge of a pull request with
  `bench-accepted` and for a `workflow_dispatch` on `main` with an input that accepts drift. The
  anchor of a class is the median of its oldest 3 samples after the newest marker. A run of `main`
  whose class baseline (with its own results as the newest sample) is beyond tolerance from the
  anchor fails, naming the marker's commit, reason and date and the commits of the anchor's samples
  (review O2); a run that writes a marker passes its drift as accepted (review O6). Pull request
  runs list drift from the stored samples only (review O9).
- **Robustness of the writing job:** a missing artefact gives a warning and still writes the marker
  (review O4); if the labels of the merged pull request cannot be read after three tries, the job
  writes nothing and fails, so that a re-run writes sample and marker together (review O5); it
  checks the target branch itself before pushing (review O8, with the test input
  `bench-data-test-…` for verification). Runs of `main` get a concurrency group per run, so that no
  queued run of `main` is cancelled (review O7).
- **Open when it comes back:** whether `actions/download-artifact` finds the artefact of the first
  attempt on "Re-run failed jobs"; tuning the window (5), the minimum (3) and the tolerance on the
  recorded data.
