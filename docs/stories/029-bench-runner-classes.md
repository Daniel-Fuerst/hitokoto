# S029 — Benchmark compared with main on the same runner

- **Status:** Done (accepted by Armin on 2026-11-09; the form is tuned from experience). In review
  since 2026-11-09 (pull request #31, CI run #125 green on Linux, Windows and macOS); confirmed by
  Armin on 2026-11-08 after review `docs/reviews/029-bench-runner-classes-spec.md`, revised after
  the code review `docs/reviews/029-bench-runner-classes-code.md` and confirmed again on
  2026-11-09 ·
  **Order:** 30 · **Size:** M · **Depends on:** S006, S007, S017 · **Decision:** ADR 0009
- **Basis:** the entry "Benchmark job on Linux red without a code change" in
  `unrefined.md` (seen since S017, refined here and removed there) and Armin's decisions of
  2026-11-08 and 2026-11-09 in conversation. The brief asks for benchmarks in CI that "fail or flag
  the build on significant regressions" (section 4); this story changes how the Linux job decides
  that, not what is measured.
- **Revision of 2026-11-09:** the first version compared with a baseline per runner class, recorded
  by CI on a branch `bench-data`, and measured main on the same runner only when the class could not
  decide. It was implemented and verified in CI; in the code review Armin chose to measure main on
  the same runner in every pull request run instead, which is as reliable in both directions and
  needs no stored data, at about 5 more minutes per run. The class design is kept in ADR 0009,
  "Deferred", and noted in `unrefined.md`.

## Story

As the person who merges pull requests, I want the Linux benchmark job to be red only for a real
regression and green only when there is none, so that a red run means work and not "restart and
hope for a faster runner", and a green run is not luck either.

## Background

`bench/baselines/linux.json` holds one CI run per key and is compared with a tolerance of 25 %. The
GitHub runners behind `ubuntu-24.04` are different machines, and their speed differs by more than
that, not by one factor for all kinds of work. The 17 Linux artefacts kept so far (2026-10-25 to
2026-11-08) show four kinds of hardware, told apart by `context.caches` and `mhz_per_cpu` of
`core-bench.json`:

| Class | Clock | L1d / L2 / L3 | Runs | Core decoding of 10 MB |
|---|---|---|---|---|
| A | 3647–3738 MHz | 32 K / 1 M / 32 M | 6 | 12.0–12.4 ms, once 17.5 ms |
| B | 3241–3265 MHz | 32 K / 512 K / 32 M | 7 | 18.5–20.4 ms |
| C | 2800 MHz | 48 K / 1.25 M / 48 M | 2 | 23.1–24.2 ms |
| D | 4304–4540 MHz | 48 K / 1 M / 32 M | 2 | 7.7–8.6 ms |

A baseline from one of them fails pull requests on slower runners and passes regressions on faster
ones: a pull request on class D can double the decoding time of main and still stay below a
baseline from class C. Within one runner, the runs of one commit agree within a few per cent on most
keys (B: three runs of pull request #24, 37 of 39 compared keys), but not on all: the openings
`s023_open_10mb_first_chunk_ms` (54, 54, 74 ms) and `s024_open_10mb_properties_ms` (31, 56, 61 ms)
swing by more than 25 % with the same code. Normalising by runner speed stays declined
(2026-10-28).

## Decisions (Armin)

1. **Main decides, on the same runner** (2026-11-09, replacing a baseline per runner class of
   2026-11-08): every pull request run builds the base commit and measures it alternating with the
   pull request on the same runner. Only a pull request slower than main on the same machine is
   red.
2. **A deliberate slowdown is accepted by a label** on the pull request; hard budgets still hold
   (2026-11-08).
3. **Toolchain changes come in their own pull request** and are compared with the base built with
   its own toolchain where that is possible (2026-11-09).
4. **Linux only.** The Windows and macOS benchmarks stay off (decision of 2026-10-28); the same
   scheme can follow there once it works on Linux.
5. **Runs of `main` check the hard budgets only** (2026-11-09); changes reach `main` through pull
   requests.

## Rules

**Runner class (information).** The job derives the class of its runner from the CPU model, the
CPU count and the sizes of the L2 and L3 caches: `linux-<model>-<n>cpu-l2-<L2>-l3-<L3>`. `<model>`
is the first `model name` value of `/proc/cpuinfo`, lower case, every run of characters other than
`a`–`z` and `0`–`9` replaced by one `-`, without leading or trailing `-`; `<n>` is the number of
CPUs the job may use (`nproc`); `<L2>` and `<L3>` are the sizes of
`/sys/devices/system/cpu/cpu0/cache/index2` and `index3` as given there, lower case. No `model name`
line, or an empty one, gives `linux-unknown-…`. The class decides nothing; it is reported, so that
a reader can place the numbers and the data exists if ADR 0009, "Deferred", comes back.

**Pull request run.** In this order:

1. **Toolchain.** A *toolchain change* is a change of the Qt version or of the runner image (`os`)
   of the bench job in `ci.yml`, of `CMakePresets.json`, or of anything under `cmake/`. If the pull
   request has a toolchain change and changes anything else than files under `docs/` and files
   named `*.md`, the job fails: "toolchain changes belong in their own pull request". It lists the
   other paths.
2. **Build of the base.** The base is the first parent of the merge commit CI tests (`HEAD^1` of
   `GITHUB_SHA`), not `pull_request.base.sha`, which GitHub does not refresh on every update. The
   job builds it from its own checkout (`git worktree`) with the `bench` preset in a separate
   directory, so its `CMakePresets.json`, `cmake/` and fetched test libraries are its own. If the Qt
   version of the bench job in the base's `ci.yml` differs from the pull request's, the job also
   installs the base's Qt and builds the base with it. If the runner image differs, the report says
   that both sides ran on the pull request's image, so a slowdown caused by the image is not
   visible.
3. **Rounds.** On the same runner the job measures four rounds of `hitokoto-bench`, in the order
   base, pull request, pull request, base. A round counts if `hitokoto-bench` wrote complete results
   (every key of the hard budgets), whatever its exit code.
4. **Hard budgets** (saving, key events above 50 ms) of the pull request's rounds fail the job,
   whatever follows. Those of the base's rounds are reported as information, with round, value and
   limit.
5. **Comparison.** A compared key fails only if both hold:
   - the mean of the pull request's two rounds is more than 25 % and more than the minimum
     difference above the mean of the base's two rounds, and
   - the pull request's faster round is slower than the base's slower round (the two sides do not
     overlap).

   The minimum difference is 2 ms; for the first screen of an opening (`open_*_first_chunk_ms`,
   `s023_open_10mb_first_chunk_ms`, `s024_open_10mb_properties_ms`) it is 20 ms, since their single
   opens lie on two levels one frame (about 17 ms) apart on one runner (Armin, 2026-11-08, after the
   first CI run on pull request #31: 48.0 against 64.6 ms without a code change). The second
   condition keeps openings that jump between two levels from failing; a real regression moves every
   round. Report-only keys (maxima, counts over 16 ms, settle times) are compared and listed but
   never fail. Keys that exist on one side only are listed for information. If the pull request
   changes anything under `bench/` (other than `bench/gate/`), the report says that the two sides
   were measured by different harnesses.
6. **Accepted slowdown:** if the pull request carries the label `bench-accepted` when the job runs,
   failures of step 5 are reported as "accepted by label" and do not fail the job; steps 1 and 4
   still do. The label is read through the REST API at run time, not from the event that started
   the run, so adding the label and re-running the job is enough.

The tolerance stays 25 %. Whether it can be tighter between two measurements on one runner is
decided later, on observed runs.

**Runs of `main`** (push to `main`, `workflow_dispatch` on `main`): the job measures once and fails
on a missed hard budget; it builds no base and compares nothing. Runs on other branches through
`workflow_dispatch` behave like pull request runs against the merge base with `main`, without the
label.

**What the job reports.** The job summary (`$GITHUB_STEP_SUMMARY`) and the log name the runner
class with model name, CPU count, cache sizes and clock, the base commit, the outcome, every key
that is not ok with the values of all four rounds and the means of both sides, missed hard budgets
of both sides, notices (toolchain, image, harness), and the label if it applied. If the base does
not build, or a round writes no complete results, the job fails and the summary says which. The
artefact holds `bench-class.json` (class and context), `core-bench.json`, `e2e-bench.json` and the
results of every round.

**Migration.** `bench/baselines/linux.json` and its comparison go away; `windows.json` and
`macos.json` stay untouched while their jobs are off. `bench/baselines/README.md` and
`docs/performance.md` describe the new scheme.

## Acceptance scenarios

**Scenario: the job knows its runner class**
- Given any run of the Linux benchmark job
- When the job has run
- Then its log, summary and artefact (`bench-class.json`) name the runner class, model name, CPU
  count, cache sizes and clock

**Scenario: a pull request as fast as main**
- Given a pull request that does not change the speed of a compared key
- When the job runs
- Then it builds the base, measures four rounds and is green, on any runner class

**Scenario: a real regression**
- Given a pull request that makes a compared key slower on the same runner, so that the mean of its
  two rounds is more than 25 % and 2 ms above the base's and its faster round is slower than the
  base's slower round
- When the job runs
- Then the job fails and names the key with the values of all four rounds and the means of both
  sides

**Scenario: an opening that jumps between two levels**
- Given a compared key whose rounds on one runner lie at two levels, for example 31 and 56 ms for
  the base and 55 and 61 ms for the pull request
- When the job compares them
- Then the key does not fail, since the two sides overlap

**Scenario: a regression on a fast runner**
- Given a pull request that doubles a compared key on a runner faster than any earlier one
- When the job runs
- Then the job fails, since the base is measured on the same runner

**Scenario: accepted slowdown**
- Given a pull request that is slower than the base commit beyond tolerance
- And it carries the label `bench-accepted`
- When the job runs, or is re-run after the label was added
- Then the job is green and lists the slower keys as accepted by label

**Scenario: hard budgets hold in every case**
- Given a run in which a key event takes more than 50 ms or saving stalls the UI for more than 16 ms
- When the job runs, with or without the label, on `main` or for a pull request
- Then the job fails

**Scenario: the base misses a hard budget**
- Given a pull request run in which a round of the base misses a hard budget and the pull request's
  rounds do not
- When the job runs
- Then the outcome is decided as without the miss, and the report lists it as information with
  round, value and limit

**Scenario: a Qt update**
- Given a pull request that only raises the Qt version of the bench job in `ci.yml`
- When the job runs
- Then the base is built with its own Qt version and the pull request with the new one, and a
  slower Qt fails the job unless the label is set

**Scenario: a toolchain change mixed with code**
- Given a pull request that changes the Qt version, the runner image of the bench job,
  `CMakePresets.json` or a file under `cmake/`, and also a file outside `docs/` not named `*.md`
- When the job runs
- Then it fails, says that toolchain changes belong in their own pull request and lists the other
  paths

**Scenario: a new runner image**
- Given a pull request that only changes the runner image of the bench job
- When the job runs
- Then the report says that both sides ran on the new image and a slowdown caused by it is not
  visible to the comparison

**Scenario: the base does not build, or a round is incomplete**
- Given a pull request run whose base commit does not build, or a round without complete results
- When the job runs
- Then it fails, and the summary says which

**Scenario: a run of main**
- Given a push to `main`
- When the Linux benchmark job runs
- Then it measures once, checks the hard budgets, and builds no base

## Requirements for the implementation

- The decisions (class from the text of `/proc/cpuinfo` and the cache sizes, toolchain change and
  the paths allowed beside it, the comparison of the rounds, report-only keys, the minimum
  differences, hard budgets of both sides, harness and image notices, the label, the outcome) are
  logic and live in C++ with the standard library only, in `bench/gate/`, unit-tested without Qt
  and built whenever tests are built, also in `core-sanitize` (ADR 0009).
- A small executable `hitokoto-bench-gate` (the gate library plus `Qt6::Core` for files and JSON, no
  `QApplication`) measures nothing and decides from given files: results of the run, the rounds of
  both sides, the changed paths, whether the Qt version or the image changed, the label. It exits
  with 0 (green) or 1 (fails), always writes the report for the log and the job summary, also when a
  round is incomplete, and prints the class (`--print-class`). It is built in the `dev`, `ci` and
  `bench` presets, and its `ctest` cases run in the `build-test` jobs. `hitokoto-bench` keeps
  measuring. The workflow only orchestrates: build, install, run.
- The workflow uses only what the runner image and the existing actions provide (`git`, `bash`,
  `gh`, `jurplel/install-qt-action`); no new action and no new dependency (brief, section 3).
- Permissions: the benchmark job reads only (`contents: read`; `issues: read` and
  `pull-requests: read` for the label).
- Changes of `ci.yml`: the bench job (class, base build with the base's Qt if it differs, four
  rounds, gate, summary), `fetch-depth: 0` (the merge base of dispatches on other branches), and
  `timeout-minutes: 30`. They need Armin's approval and are copied by hand (the device bridge
  refuses `.github/`). The concurrency of `ci.yml` stays as on `main`.
- Cost: one build of the base and four more runs of `hitokoto-bench` in every pull request run
  (7 min 52 s for the bench job on pull request #31, reported in `docs/performance.md`); the core
  benchmarks are not run for the base.
- Theming and S009 are not affected: the story adds no visible element and conceals no syntax.

## Tests

- Gate tests `S029_RunnerClasses` (GoogleTest, standard library only, `tests/bench/`): class from
  `/proc/cpuinfo` text and cache sizes (normalisation, several CPUs, missing or empty model name);
  toolchain change (Qt version, image, `CMakePresets.json`, `cmake/`) and the paths allowed beside
  it; the comparison (mean of two rounds, no overlap, minimum differences including the first
  screen, report-only keys, keys on one side only, the four round values in the result); hard
  budgets of the pull request's rounds (fail) and of the base's rounds (information); harness and
  image notices; the label (comparisons accepted, hard budgets and mixed toolchain changes not);
  runs of `main` (hard budgets only).
- `ctest` cases that run `hitokoto-bench-gate` on fixed files in `tests/bench/data/` and check exit
  code and report for: green, regression (with the four round values), two-level opening, label,
  hard budget of the pull request, hard budget of the base, incomplete round, toolchain change
  alone, toolchain change mixed with code, image change, run of `main`, class output.
- In CI, on the pull request of this story: the job builds the base, measures four rounds and is
  green. The Qt update and the image change are covered by the `ctest` cases; they are checked in
  CI with the next real update.

## Review

Spec review: `docs/reviews/029-bench-runner-classes-spec.md` (12 objections, dispositions there; it
reviewed the first version). Code review: `docs/reviews/029-bench-runner-classes-code.md`.
