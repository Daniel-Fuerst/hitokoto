# S029 — Benchmark baselines per runner class, recorded by CI

- **Status:** Open (written 2026-11-08, waiting for confirmation after the spec review) ·
  **Order:** 30 · **Size:** M · **Depends on:** S006, S007, S017 · **Decision:** ADR 0009
  (Proposed)
- **Basis:** the entry "Benchmark job on Linux red without a code change" in
  `unrefined.md` (seen since S017, refined here and removed there) and Armin's decisions of
  2026-11-08 in conversation. The brief asks for benchmarks in CI that "fail or flag the build on
  significant regressions" (section 4); this story changes how the Linux job decides that, not
  what is measured.

## Story

As the person who merges pull requests, I want the Linux benchmark job to be red only for a real
regression, so that a red run means work and not "restart and hope for a faster runner", and a new
kind of runner never goes unnoticed.

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

Within a class the runs of one commit agree within a few per cent (B: three runs of pull request
#24), with one exception: the run of pull request #26 had the caches and clock of A but decoded in
17.5 ms. Between classes there is no common factor (C against B: decoding ×1.17, rendering ×0.81,
typing ×1.4), so normalising by runner speed stays declined (2026-10-28).

## Decisions (Armin, 2026-11-08)

1. **A baseline per runner class**, not one for all runners. The job knows the class of its runner
   and compares with the baseline of that class.
2. **The data comes from CI, automatically.** Runs of the code on `main` add a sample to their
   class; nobody copies artefacts by hand. Pull request runs never add samples: their code is not
   yet accepted.
3. **A class needs a minimum number of samples** before its baseline decides anything.
4. **When the class baseline cannot decide, main decides:** if the class has too few samples, or the
   pull request is slower than the class baseline, the job builds the base commit and measures it on
   the same runner, alternating with the pull request. Only a pull request slower than main on the
   same machine is red.
5. **A deliberate slowdown is accepted by a label** on the pull request; hard budgets still hold.
6. **Linux only.** The Windows and macOS benchmarks stay off (decision of 2026-10-28); the same
   scheme can follow there once it works on Linux.

## Rules

**Runner class.** The class is derived on the runner from `/proc/cpuinfo` and the CPU count:
`linux-<model>-<n>cpu`, where `<model>` is the first `model name` value, lower case, every run of
characters other than `a`–`z` and `0`–`9` replaced by one `-`, without leading or trailing `-`
(for example `linux-amd-epyc-7763-64-core-processor-4cpu`). No `model name` line, or an empty one,
gives the class `linux-unknown-<n>cpu`, which never becomes established (rule below) and so always
compares with main. Clock and cache sizes are recorded beside the class for information; they are
not part of it, since the clock of one model varies between runs (A: 3647–3738 MHz).

**Sample.** The values of `e2e-bench.json` from one run of the code on `main`, with its context:
class, model name, CPU count, clock, cache sizes, commit, run ID and attempt, event (`push`,
`schedule`, `workflow_dispatch`) and UTC time. Hard-budget failures of that run do not stop it from
being recorded: the baseline is a median, and one disturbed run does not move it.

**Storage.** Samples live on the branch `bench-data` of the repository, created as an orphan branch
(no common history with `main`), one file per sample:
`linux/<class>/<UTC time>-<short commit>-<run ID>-<attempt>.json`. Only CI writes there; nothing on
`main` changes when a sample is added, and no CI run is triggered by it (`ci.yml` runs on pushes to
`main` only).

**Established class and its baseline.** A class is established when it holds at least
**3** samples. Its baseline is, per key, the median over the newest **5** samples (by UTC time) that
contain the key; a key found in fewer than 3 of them has no class baseline yet. Minimum and window
are constants in one place.

**Pull request run.** In this order:

1. **Hard budgets** of the pull request's run are checked as today (saving, key events above 50 ms);
   a miss fails the job whatever follows.
2. **Class stage**, when the class is established: every compared key with a class baseline is
   compared as today (fails at more than 25 % and more than 2 ms slower; report-only keys stay
   report-only). Keys without a class baseline are listed as "no class baseline yet", for
   information. All compared keys within tolerance: the job is green, and the comparison with main
   is skipped.
3. **Main stage**, when the class is not established or the class stage found a key slower than
   tolerance: the job builds the base commit of the pull request (`pull_request.base.sha`, the first
   parent of the merge commit CI tests) with the `bench` preset in a separate directory and runs
   `hitokoto-bench` alternately: pull request (the run of step 1), base, pull request, base. Per key
   and side the lower of the two round values counts. A compared key fails when the pull request is
   more than 25 % and more than 2 ms slower than the base. Keys that exist on one side only are
   listed for information. If the pull request changes anything under `bench/`, the job log says
   so: the two sides were measured by different harnesses.
4. **Accepted slowdown:** if the pull request carries the label `bench-accepted` when the job runs,
   failures of steps 2 and 3 are reported as "accepted by label" and do not fail the job; step 1
   still does. The label is read through the REST API at run time, not from the event that started
   the run, so adding the label and re-running the job is enough.

The tolerance stays 25 % in both stages. Whether it can be tighter within a class or between two
measurements on one runner is decided later, on the recorded data.

**Runs of `main`** (push to `main`, the nightly schedule, `workflow_dispatch` on `main`): hard
budgets fail the job as today; the class stage runs and is reported but never fails the job (the
code is already merged, and a slower class baseline is the expected result after an accepted
slowdown); the main stage does not run; afterwards one sample is added to `bench-data`. Runs on
other branches through `workflow_dispatch` behave like pull request runs against the merge base with
`main`.

**Nightly run.** `ci.yml` also runs on a schedule once a night on `main`, so that rare classes (C
and D: 2 of 17 runs each) reach 3 samples in weeks, not months, without anybody starting runs. In a
public repository GitHub disables scheduled workflows after 60 days without repository activity;
that is acceptable, since then there is nothing to measure.

**What the job reports.** The job summary (`$GITHUB_STEP_SUMMARY`) and the log name the class, its
number of samples, the stage that decided and why (for example "class has 1 of 3 samples, compared
with main on this runner"), every failed key with both numbers, and the label if it applied. The
artefact holds `bench-class.json` (class and context) besides `core-bench.json` and
`e2e-bench.json`, and in the main stage the results of every round.

**Failures of the mechanism.** If `bench-data` cannot be fetched, or a sample cannot be read, the
job warns and treats the class as not established (main stage); it never passes a pull request
without a comparison. If the base commit does not build or its benchmark does not run, the job
fails and says so. If adding a sample fails after three attempts (another run pushed meanwhile), the
job warns; the next run of `main` adds its own sample.

**Migration.** `bench-data` starts empty, so until classes are established every pull request runs
the main stage. `bench/baselines/linux.json` and its comparison go away; `windows.json` and
`macos.json` stay untouched while their jobs are off. `bench/baselines/README.md` and
`docs/performance.md` describe the new scheme.

## Acceptance scenarios

**Scenario: the job knows its runner class**
- Given any run of the Linux benchmark job
- When the job has run
- Then its log, summary and artefact (`bench-class.json`) name the runner class, model name, CPU
  count, clock and cache sizes

**Scenario: a run of main adds a sample**
- Given a push to `main` (or the nightly run, or `workflow_dispatch` on `main`)
- When the Linux benchmark job finishes
- Then `bench-data` holds a new file under `linux/<class>/` with the values of `e2e-bench.json` and
  the context of the run
- And no file on `main` changed and no further CI run started

**Scenario: pull request runs add nothing**
- Given a pull request, from a branch of this repository or from a fork
- When the Linux benchmark job finishes
- Then `bench-data` is unchanged, and the job had no write access to the repository

**Scenario: established class, within tolerance**
- Given the runner's class holds at least 3 samples
- And no compared key of the pull request is more than 25 % and 2 ms slower than the class baseline
- When the job runs
- Then it is green after the class stage, and the base commit is not built

**Scenario: established class, slower than its baseline but not than main**
- Given the class stage finds a compared key more than 25 % and 2 ms slower than the class baseline
- And on the same runner the base commit is as slow (within 25 % or 2 ms)
- When the job runs
- Then the job is green and reports that the class baseline was exceeded and main on this runner was
  as slow

**Scenario: a real regression**
- Given a pull request that makes a compared key more than 25 % and 2 ms slower than the base commit
  on the same runner
- When the job runs
- Then the job fails and names the key with the values of both sides

**Scenario: a class with too few samples**
- Given the runner's class holds fewer than 3 samples (or is `linux-unknown-…`)
- When the job runs for a pull request
- Then the class stage is skipped, the main stage decides, and the summary says how many samples
  the class holds

**Scenario: accepted slowdown**
- Given a pull request that is slower than the base commit beyond tolerance
- And it carries the label `bench-accepted`
- When the job runs, or is re-run after the label was added
- Then the job is green and lists the slower keys as accepted by label

**Scenario: hard budgets hold in every case**
- Given a run in which a key event takes more than 50 ms or saving stalls the UI for more than 16 ms
- When the job runs, with or without the label, in any stage, on `main` or for a pull request
- Then the job fails

**Scenario: `bench-data` unavailable**
- Given `bench-data` cannot be fetched or holds a sample that cannot be read
- When the job runs for a pull request
- Then it warns, and the main stage decides

**Scenario: main absorbs an accepted slowdown**
- Given a pull request merged with `bench-accepted`
- When the following runs of `main` add samples to a class
- Then the class baseline follows the new code once the new samples are the majority of the
  window, and until then pull requests on that class that exceed it are decided by the main stage

## Requirements for the implementation

- The decisions (class from `/proc/cpuinfo` text, established class, baseline per key, the two
  stages, report-only keys, the label, the exit reason) are logic and live in C++ with the standard
  library only, unit-tested without Qt (ADR 0009). `hitokoto-bench` keeps the measuring and does the
  file and JSON work with Qt, as today.
- `hitokoto-bench` gains an evaluating mode that measures nothing and decides from given files
  (results, class samples, base rounds, label), so that the decision is tested in `ctest` with fixed
  files, and the workflow only orchestrates: build, run, fetch, push.
- The workflow uses only what the runner image and the existing actions provide (`git`, `bash`,
  `gh`); no new action and no new dependency (brief, section 3).
- Permissions: the benchmark job reads (`contents: read`, `pull-requests: read` for the label); only
  the step or job that adds a sample on `main` has `contents: write`, and it runs only for `push`,
  `schedule` and `workflow_dispatch` on `main`. Adding samples is serialised by its own concurrency
  group, never cancelled, and retried with a rebase on `bench-data`.
- A change of `ci.yml` needs Armin's approval and is copied by hand (the device bridge refuses
  `.github/`).
- The main stage at most doubles the measuring part of the job plus one build; its duration is
  measured on the pull request of this story and reported in `docs/performance.md`.
- Theming and S009 are not affected: the story adds no visible element and conceals no syntax.

## Tests

- Core tests `S029_RunnerClasses` (GoogleTest, standard library only): class from `/proc/cpuinfo`
  text (normalisation, several CPUs, missing or empty model name, CPU count); established class at 3
  samples; baseline as median of the newest 5 per key, keys in fewer than 3 samples without baseline,
  ordering by time; the class stage (tolerance, 2 ms rule, report-only keys, keys without baseline);
  the main stage (lower of two rounds per side, keys on one side only); the decision for `main` runs
  (class stage reported only); the label (comparison accepted, hard budgets not); unreadable samples
  treated as missing.
- `ctest` cases that run `hitokoto-bench` in its evaluating mode on fixed files in
  `tests/bench/data/` and check exit code and the reported stage for: established and green,
  established and red then green against main, red against main, too few samples, label, hard budget.
- In CI, on the pull request of this story: a run without samples (main stage), and through
  `workflow_dispatch` on the story branch with a test target branch instead of `bench-data` (an
  input that exists for this purpose and is refused on `main`): samples added, then a run that
  finds its class established. The test branch is deleted afterwards.

## Open for the review

- Minimum 3 and window 5, the median, and one tolerance of 25 % for both stages.
- Where the gate logic lives (ADR 0009: a small standard-library module beside the benchmarks, or
  `src/core`).
- The nightly run, and whether pull request runs should add the base commit's rounds of the main
  stage as samples (they are main code on that runner and would fill rare classes faster, but need
  write access in pull request runs).
