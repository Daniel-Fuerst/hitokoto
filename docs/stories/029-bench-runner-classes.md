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

Within a class the runs of one commit agree within a few per cent on most keys (B: three runs of
pull request #24, 37 of 39 compared keys), but not on all: the openings
`s023_open_10mb_first_chunk_ms` (54, 54, 74 ms) and `s024_open_10mb_properties_ms` (31, 56, 61 ms)
swing by more than 25 % on one runner class with the same code, and so did both in the two class A
runs of #24; the run of pull request #26 had the caches and clock of A but decoded in 17.5 ms.
Between classes there is no common factor (C against B: decoding ×1.17, rendering ×0.81, typing
×1.4), so normalising by runner speed stays declined (2026-10-28).

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
7. **An anchor per class catches slow drift** (spec review O2): since class baselines follow `main`,
   many small slowdowns could add up unnoticed. Each class has an anchor that moves only with a
   deliberately accepted slowdown; a run of `main` whose class baseline has drifted more than the
   tolerance from the anchor is red.

## Rules

**Runner class.** The class is derived on the runner from the CPU model, the CPU count and the
sizes of the L2 and L3 caches: `linux-<model>-<n>cpu-l2-<L2>-l3-<L3>`. `<model>` is the first
`model name` value of `/proc/cpuinfo`, lower case, every run of characters other than `a`–`z` and
`0`–`9` replaced by one `-`, without leading or trailing `-`; `<n>` is the number of CPUs the job
may use (`nproc`); `<L2>` and `<L3>` are the sizes of `/sys/devices/system/cpu/cpu0/cache/index2`
and `index3` as given there (for example `512K`, `32768K`), lower case. Example:
`linux-amd-epyc-7763-64-core-processor-4cpu-l2-512k-l3-32768k`. The caches are part of the class
because they separated A–D in the artefacts while no model name was recorded yet (review O4); if a
hypervisor reports one model name for different hardware, the caches still keep them apart. No
`model name` line, or an empty one, gives `linux-unknown-…`, which never becomes established (rule
below) and so always compares with main. The clock is recorded for information only, since it varies
within one model (A: 3647–3738 MHz).

**Sample.** The values of `e2e-bench.json` from one run of the code on `main`, with its context:
class, model name, CPU count, clock, cache sizes, commit, run ID and attempt, event (`push`,
`schedule`, `workflow_dispatch`) and the start time of the run (`run_started_at`, UTC). A sample is
written only if `hitokoto-bench` finished and wrote complete results (review O8): the writer checks
that the file parses, holds the context fields and at least every key of the hard budgets. A missed
hard budget does not stop a complete run from being recorded: the baseline is a median, and one
disturbed run does not move it.

**Storage.** Samples live on the branch `bench-data` of the repository, created as an orphan branch
(no common history with `main`), one file per sample:
`linux/<class>/<start time>-<short commit>-<run ID>-<attempt>.json`. Only CI writes there; nothing
on `main` changes when a sample is added, and no CI run is triggered by it (`ci.yml` runs on pushes
to `main` only).

**Established class and its baseline.** A class is established when it holds at least
**3** readable samples. Its baseline is, per key, the median over the newest **5** samples (by start
time) that contain the key; a key found in fewer than 3 of them has no class baseline yet. Minimum
and window are constants in one place. A sample that cannot be read is skipped and counted as
missing, with a warning that names the file (review O8).

**Anchor and drift.** An *acceptance marker* is a file `linux/accepted/<start time>-<short
commit>.json` on `bench-data`. The job that adds samples writes one when the commit of a push to
`main` is the merge of a pull request that carried `bench-accepted` (looked up through the REST API,
`commits/<sha>/pulls`), or when `workflow_dispatch` on `main` is started with the input that accepts
the current drift. The *anchor* of a class is, per key, the median over the oldest 3 samples of the
class that are newer than the newest marker (without a marker: its oldest 3 samples). While a class
has fewer than 3 samples after the newest marker, it has no anchor and nothing is checked for drift.
A compared key *has drifted* when its class baseline, computed with the run's own results as the
newest sample, is more than 25 % and more than 2 ms above its anchor. A run of `main` with drift
fails and names the keys, anchor, baseline and the commit since which the anchor holds; pull request
runs list the drift of their class for information, since the pull request did not cause it. Drift
is resolved by finding the cause, or, if the slowdown is wanted, by accepting it through the
`workflow_dispatch` input; the anchor then forms anew from the next 3 samples of each class.

**Pull request run.** In this order:

1. **Hard budgets** of the pull request's run are checked as today (saving, key events above 50 ms);
   a miss fails the job whatever follows.
2. **Class stage**, when the class is established: every compared key with a class baseline is
   compared as today (fails at more than 25 % and more than 2 ms slower; report-only keys stay
   report-only). Keys without a class baseline are listed as "no class baseline yet", for
   information. All compared keys within tolerance: the job is green, and the comparison with main
   is skipped.
3. **Main stage**, when the class is not established or the class stage found a key slower than
   tolerance. The base is the first parent of the merge commit CI tests (`HEAD^1` of `GITHUB_SHA`,
   fetched with `fetch-depth: 2`; review O3), not `pull_request.base.sha`, which GitHub does not
   refresh on every update. The job builds the base with the `bench` preset in a separate directory
   and then measures four fresh rounds in the order base, pull request, pull request, base (review
   O5; the run of step 1 counts only for hard budgets and the class stage). A compared key fails
   only if both hold:
   - the mean of the pull request's two rounds is more than 25 % and more than 2 ms above the mean
     of the base's two rounds, and
   - the pull request's faster round is slower than the base's slower round (the two sides do not
     overlap).

   The second condition keeps openings that jump between two levels on one runner from failing
   (Background; review O1); a real regression moves every round. Keys that exist on one side only
   are listed for information. Hard budgets of the pull request's rounds fail the job; those of the
   base's rounds are information (review O9). A round counts if `hitokoto-bench` wrote complete
   results, whatever its exit code. If the pull request changes anything under `bench/`, the log
   says that the two sides were measured by different harnesses; if it changes the toolchain (the
   Qt version or other steps of `ci.yml`, `CMakePresets.json`, `cmake/`), the log says that both
   sides were built with the pull request's toolchain, and a failure of the class stage then stands
   unless the label is set (review O6).
4. **Accepted slowdown:** if the pull request carries the label `bench-accepted` when the job runs,
   failures of steps 2 and 3 are reported as "accepted by label" and do not fail the job; step 1
   still does. The label is read through the REST API at run time, not from the event that started
   the run, so adding the label and re-running the job is enough.

The tolerance stays 25 % in both stages. Whether it can be tighter within a class or between two
measurements on one runner is decided later, on the recorded data.

**Runs of `main`** (push to `main`, the nightly schedule, `workflow_dispatch` on `main`): hard
budgets fail the job as today; drift fails it (above); the class stage runs and is reported but
never fails the job (the code is already merged, and a slower class baseline is the expected result
after an accepted slowdown); the main stage does not run; afterwards one sample is added to
`bench-data`. Runs on other branches through `workflow_dispatch` behave like pull request runs
against the merge base with `main` and add nothing, except for the test input below.

**Cancelling.** Today `ci.yml` cancels a running workflow when a newer one starts on the same ref,
on `main` too, so a second merge would cancel the first merge's sample (review O10). With this story
only pull request runs are cancelled that way (`cancel-in-progress` true for `pull_request` only);
runs of `main` finish. The job that adds a sample has no concurrency group of its own: every sample
is a new file, so two pushes never conflict, and a rejected push is retried with a rebase.

**Nightly run.** `ci.yml` also runs on a schedule once a night on `main`, so that rare classes (C
and D: 2 of 17 runs each) reach 3 samples in weeks, not months, without anybody starting runs. It
runs the whole workflow, which doubles as a nightly check of `main` against changed runner images;
a failure of another job (`build-test` on Windows, say) costs that night's sample. In a public
repository GitHub disables scheduled workflows after 60 days without repository activity; that is
acceptable, since then there is nothing to measure.

**Test input.** For the verification of this story, `workflow_dispatch` takes an input naming a
branch to write samples to instead of `bench-data`. It is accepted only if the name starts with
`bench-data-test-` and the run is not on `main`; then, and only then, the job that adds a sample
also runs for that dispatch on another branch. This is the one exception to "only runs of `main`
write", limited to people who can start workflows (review O11).

**What the job reports.** The job summary (`$GITHUB_STEP_SUMMARY`) and the log name the class, its
number of samples, the stage that decided and why (for example "class has 1 of 3 samples, compared
with main on this runner"), the base commit in the main stage, every failed key with the values of
both sides, drift of the class, and the label if it applied. The
artefact holds `bench-class.json` (class and context) besides `core-bench.json` and
`e2e-bench.json`, and in the main stage the results of every round.

**Failures of the mechanism.** If `bench-data` cannot be fetched, the job warns and treats the class
as not established (main stage); unreadable samples are skipped (above). The job never passes a pull
request without a comparison. If the base commit does not build, or a round writes no complete
results, the job fails and says so. If adding a sample fails after three attempts (another run
pushed meanwhile), the job warns; the next run of `main` adds its own sample.

**Migration.** `bench-data` starts empty, so until classes are established every pull request runs
the main stage. `bench/baselines/linux.json` and its comparison go away; `windows.json` and
`macos.json` stay untouched while their jobs are off. `bench/baselines/README.md` and
`docs/performance.md` describe the new scheme.

## Acceptance scenarios

**Scenario: the job knows its runner class**
- Given any run of the Linux benchmark job
- When the job has run
- Then its log, summary and artefact (`bench-class.json`) name the runner class, model name, CPU
  count, cache sizes and clock

**Scenario: a run of main adds a sample**
- Given a push to `main` (or the nightly run, or `workflow_dispatch` on `main`)
- When the Linux benchmark job finishes
- Then `bench-data` holds a new file under `linux/<class>/` with the values of `e2e-bench.json` and
  the context of the run
- And no file on `main` changed and no further CI run started
- And a second merge shortly after the first does not cancel the first one's run

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
- And on the same runner the base commit is about as slow
- When the job runs
- Then the job is green and reports that the class baseline was exceeded and main on this runner was
  as slow, with the base commit it measured

**Scenario: a real regression**
- Given a pull request that makes a compared key slower on the same runner, so that the mean of its
  two rounds is more than 25 % and 2 ms above the base's and its faster round is slower than the
  base's slower round
- When the job runs
- Then the job fails and names the key with the values of every round of both sides

**Scenario: an opening that jumps between two levels**
- Given a compared key whose rounds on one runner lie at two levels, for example 31 and 56 ms for
  the base and 55 and 61 ms for the pull request
- When the main stage compares them
- Then the key does not fail, since the two sides overlap

**Scenario: the toolchain changes**
- Given a pull request that raises the Qt version in `ci.yml`
- And the class stage finds a compared key slower than tolerance
- When the job runs
- Then the job fails and says that both sides of the main stage were built with the new toolchain,
  unless the pull request carries the label

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
- Given `bench-data` cannot be fetched
- When the job runs for a pull request
- Then it warns, and the main stage decides

**Scenario: an unreadable sample**
- Given `bench-data` holds a sample of the runner's class that cannot be read
- When the job runs
- Then it warns and names the file, and the class is judged on its other samples

**Scenario: main absorbs an accepted slowdown**
- Given a pull request merged with `bench-accepted`
- When the following runs of `main` add samples to a class
- Then the class baseline follows the new code once the new samples are the majority of the
  window, and until then pull requests on that class that exceed it are decided by the main stage

**Scenario: drift on main**
- Given a class with an anchor, and several merges each slower on a compared key by less than the
  tolerance
- When a run of `main` finds the class baseline of that key more than 25 % and 2 ms above the anchor
- Then the run fails and names the key, anchor, baseline and the commit since which the anchor holds
- And pull request runs on that class list the drift without failing because of it

**Scenario: an accepted slowdown moves the anchor**
- Given a pull request merged with `bench-accepted`, or the drift accepted through
  `workflow_dispatch` on `main`
- When the job that adds samples runs for that push or dispatch
- Then it writes an acceptance marker, each class has no anchor until it has 3 newer samples, and
  then its anchor is the median of those

**Scenario: after the merge of this story**
- Given this story is merged
- When the first push to `main` and the first nightly run have finished
- Then each has added one sample to `bench-data` (checked by hand before the story goes to Done)

## Requirements for the implementation

- The decisions (class from the text of `/proc/cpuinfo` and the cache sizes, established class,
  baseline per key, the two stages, report-only keys, toolchain and harness notices, the label, the
  outcome) are logic and live in C++ with the standard library only, in `bench/gate/`, unit-tested
  without Qt and built whenever tests are built, also in `core-sanitize` (ADR 0009).
- A small executable `hitokoto-bench-gate` (the gate library plus `Qt6::Core` for files and JSON, no
  `QApplication`) measures nothing and decides from given files: results of the pull request, the
  class samples, the base rounds, the changed paths, the label. It exits with 0 (green), 1 (fails)
  or 3 (class stage cannot decide: run the main stage) and writes the report for the log and the job
  summary (review O7). It is built in the `dev`, `ci` and `bench` presets, and its `ctest` cases run
  in the `build-test` jobs. `hitokoto-bench` keeps measuring and gains only the output of the class
  context. The workflow only orchestrates: build, run, fetch, push.
- The workflow uses only what the runner image and the existing actions provide (`git`, `bash`,
  `gh`); no new action and no new dependency (brief, section 3).
- Permissions: the benchmark job reads (`contents: read`, `pull-requests: read` for the label). Only
  a separate job that adds a sample has `contents: write`; it runs only for `push`, `schedule` and
  `workflow_dispatch` on `main`, and for the test input above. It takes the results from the
  benchmark job's artefact, validates them and pushes one new file, retrying with a rebase.
- Changes of `ci.yml`: the bench job (class, two stages, drift, label, summary), the new job that
  adds samples and acceptance markers, the nightly schedule, the inputs for testing and for
  accepting drift, `fetch-depth: 2`, and `cancel-in-progress` for pull
  requests only. They need Armin's approval and are copied by hand (the device bridge refuses
  `.github/`).
- Cost of the main stage: one build of the base and four more runs of `hitokoto-bench`, so the
  measuring part grows to five times today's in those runs (review O12); the core benchmarks are not
  run for the base. Its duration is measured on the pull request of this story and reported in
  `docs/performance.md`; the bench job gets a `timeout-minutes` derived from it.
- Theming and S009 are not affected: the story adds no visible element and conceals no syntax.

## Tests

- Gate tests `S029_RunnerClasses` (GoogleTest, standard library only, `tests/bench/`): class from
  `/proc/cpuinfo` text and cache sizes (normalisation, several CPUs, missing or empty model name);
  established class at 3 readable samples; baseline as median of the newest 5 per key, keys in fewer
  than 3 samples without baseline, ordering by start time, unreadable samples skipped; the class
  stage (tolerance, 2 ms rule, report-only keys, keys without baseline); the main stage (mean of two
  rounds, no overlap, keys on one side only, two-level openings, hard budgets of the base as
  information); toolchain and harness notices; anchor (oldest 3 after the newest marker, none while
  fewer) and drift (fails `main` runs, information in pull request runs); the decision for `main`
  runs (class stage reported only); the label (comparisons accepted, hard budgets not); sample
  validation for the writer.
- `ctest` cases that run `hitokoto-bench-gate` on fixed files in `tests/bench/data/` and check exit
  code and report for: established and green, established and red then main stage, main stage green,
  main stage red, too few samples, unreadable sample, toolchain change, label, hard budget, drift
  on `main`, drift after a marker.
- In CI, on the pull request of this story: a run without samples (main stage decides), and two
  `workflow_dispatch` runs on the story branch with the test input: the first adds a sample to the
  test branch, the second reads it and reports its class with 1 or 2 samples. The established class
  is covered by the `ctest` cases, since the runner class of a run cannot be chosen. The test branch
  is deleted afterwards. Pushes and the nightly run start from `main` only and are checked after the
  merge (last scenario).

## Review

Spec review: `docs/reviews/029-bench-runner-classes-spec.md` (12 objections, dispositions there).
