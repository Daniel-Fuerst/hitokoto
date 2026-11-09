---
story: S029
title: "Objection record: S029 benchmark baselines per runner class, recorded by CI"
mode: code
spec: docs/stories/029-bench-runner-classes.md
branch: agents/s029-bench-runner-classes
commits_examined: "705c578..ab76b11 (521e83e red tests for the gate .. ab76b11 bench job timeout); code: bench/gate/, bench/hitokoto_bench.cpp, .github/workflows/ci.yml, tests/bench/"
date: 2026-11-09
reviewer: Advocatus Diaboli (Claude, in the session of the CI verification)
dispositioned: 2026-11-09 (Armin, one objection at a time; during the disposition Armin replaced the runner classes by measuring main on the same runner in every pull request run, ADR 0009 revision)
objections:
  - id: O1
    category: failure
    severity: major
    title: "A failing main stage names only the means, not the values of every round"
    disposition: fixed
    disposition_rationale: "The report of the comparison with main shows, for every key that is not ok, the four rounds in measuring order (base 1, pull request 1, pull request 2, base 2) and the means of both sides (Armin). Story scenario \"a real regression\"; gate test TheResultCarriesTheFourRoundsInMeasuringOrder, ctest case S029_GateCase_regression. The minimum and maximum of the class window that Armin added belong to the class stage and are kept in ADR 0009, \"Deferred\"."
  - id: O2
    category: failure
    severity: major
    title: "Drift does not name the commit since which the anchor holds"
    disposition: resolved
    disposition_rationale: "No anchor and no drift after the revision of ADR 0009 (main on the same runner in every pull request run). The disposition for the class variant (name the marker commit, reason and date and the commits of the anchor samples) is kept in ADR 0009, \"Deferred\"."
  - id: O3
    category: failure
    severity: minor
    title: "Hard budgets missed by the base's rounds are not reported at all"
    disposition: fixed
    disposition_rationale: "Hard budgets missed by the base rounds are reported as information with round, value and limit; the outcome does not change (Armin). Story scenario \"the base misses a hard budget\"; gate test HardBudgetsOfThePullRequestFailThoseOfTheBaseAreInformation, ctest case S029_GateCase_base_hard_budget."
  - id: O4
    category: failure
    severity: minor
    title: "Without an artefact the record job fails and loses the acceptance marker"
    disposition: resolved
    disposition_rationale: "No record job after the revision of ADR 0009. Disposition for the class variant (tolerant download with a warning, marker without artefact) kept in ADR 0009, \"Deferred\"."
  - id: O5
    category: failure
    severity: minor
    title: "A failed label lookup silently writes no acceptance marker"
    disposition: resolved
    disposition_rationale: "No record job after the revision of ADR 0009. Disposition for the class variant (all or nothing: three tries, otherwise nothing written and the job fails, so that a re-run writes sample and marker together) kept in ADR 0009, \"Deferred\"."
  - id: O6
    category: operational
    severity: minor
    title: "The run that accepts drift is itself red"
    disposition: resolved
    disposition_rationale: "No drift after the revision of ADR 0009. Disposition for the class variant (a run that writes a marker passes its drift as accepted) kept in ADR 0009, \"Deferred\"."
  - id: O7
    category: failure
    severity: minor
    title: "A third run of main cancels the pending second one and its sample"
    disposition: resolved
    disposition_rationale: "Runs of main no longer leave samples; the concurrency of ci.yml stays as on main. Disposition for the class variant (a concurrency group per run for runs of main) kept in ADR 0009, \"Deferred\"."
  - id: O8
    category: threat
    severity: minor
    title: "The record job does not check the test branch name itself"
    disposition: resolved
    disposition_rationale: "No job with write access after the revision of ADR 0009. Disposition for the class variant (the record job checks the target branch itself) kept in ADR 0009, \"Deferred\"."
  - id: O9
    category: design
    severity: minor
    title: "Drift listed in a pull request run includes the pull request's own results"
    disposition: resolved
    disposition_rationale: "No drift after the revision of ADR 0009. Disposition for the class variant (drift in pull request runs from stored samples only) kept in ADR 0009, \"Deferred\"."
  - id: O10
    category: design
    severity: minor
    title: "The unknown class gets samples, an anchor and a drift check across different hardware"
    disposition: resolved
    disposition_rationale: "No samples, anchor or drift after the revision of ADR 0009; the class is only reported. Disposition for the class variant (unknown class: no sample, no anchor, no drift) kept in ADR 0009, \"Deferred\"."
  - id: O11
    category: operational
    severity: minor
    title: "Failures of the main stage itself reach the log but not the job summary"
    disposition: fixed
    disposition_rationale: "The gate writes a report with the reason when a round has no complete results, and the workflow writes \"the base commit <sha> does not build\" into the summary (Armin). Story scenario \"the base does not build, or a round is incomplete\"; ctest case S029_GateCase_incomplete_round, ci.yml step \"Build the base commit\"."
  - id: O12
    category: operational
    severity: minor
    title: "Runs of main show SLOW in the class table next to a green outcome"
    disposition: resolved
    disposition_rationale: "No class stage after the revision of ADR 0009. Disposition for the class variant (runs of main show the class stage as information, not SLOW) kept in ADR 0009, \"Deferred\"."
---

# Objection record: S029 benchmark baselines per runner class (code)

Mode `code`. Examined: the gate library (`bench/gate/include/hitokoto/bench_gate/gate.hpp`,
`bench/gate/src/gate.cpp`), the executable (`bench/gate/src/hitokoto_bench_gate.cpp`), the bench and
record jobs of `.github/workflows/ci.yml`, the GoogleTests `S029_RunnerClasses` and the `ctest`
cases in `tests/bench/data/`, against the story and ADR 0009. CI evidence: PR run #121 (green, main
stage 7 min 52 s), dispatches 42532362126 and 42575960727 on the story branch with
`bench_data_branch = bench-data-test-s029` (both green; samples faa0741 and d88ce22 in the class
`linux-amd-epyc-7763-64-core-processor-4cpu-l2-512k-l3-32768k`; the second reported
"1 samples, not established").

## O1 - failure - major

**A failing main stage names only the means, not the values of every round.**

The scenario "a real regression" requires that the job "fails and names the key with the values of
every round of both sides". `KeyResult` carries only `was` and `now` (`gate.hpp`), and
`compareRounds` fills them with the means of the two rounds (`gate.cpp:327-330`). `writeKeys`
prints those two numbers (`hitokoto_bench_gate.cpp:156-174`, columns "Base (mean)" and "Pull
request (mean)"). The four round values are lost before the report is written; they exist only in
the artefact (`base-*.json`, `pr-*.json`). The second condition of the main stage (no overlap) can
therefore not be checked from the summary: a reader sees two means 30 % apart and cannot tell
whether the sides overlapped. The `ctest` case `main_stage_regression` checks only the exit code,
`Stage: main` and the key name, so the gap is untested.

## O2 - failure - major

**Drift does not name the commit since which the anchor holds.**

The rule "Anchor and drift" and the scenario "drift on main" require that a failing run of `main`
"names the key, anchor, baseline and the commit since which the anchor holds". The report prints a
table "Drift from the anchor" with anchor and class baseline only (`hitokoto_bench_gate.cpp:223-225`).
The commit is available twice but read nowhere: the acceptance marker carries `commit`
(`ci.yml`, record job), but `readMarkers` keeps only `start_time` (`hitokoto_bench_gate.cpp:89-102`);
every sample carries `commit`, but `readSamples` ignores it (`:69-87`) and `anchor()` returns values
only (`gate.cpp:260-289`). Whoever has to find the cause of drift has no starting point in the
summary. The `ctest` case `drift_main` checks `Drift` and the key name, not the commit.

## O3 - failure - minor

**Hard budgets missed by the base's rounds are not reported at all.**

The story (review O9) says: "Hard budgets of the pull request's rounds fail the job; those of the
base's rounds are information." `decide` adds the misses of the pull request's rounds
(`gate.cpp:415-417`) and does nothing with the base's rounds; `Decision` has no field for them. A
base that already misses a budget, which explains a failing pull request round on a slow runner, is
invisible in the report. The information is not merely non-failing, it is absent.

## O4 - failure - minor

**Without an artefact the record job fails and loses the acceptance marker.**

`bench-record` runs on `always() && needs.bench.result != 'cancelled'` (`ci.yml:322-325`), so also
when `bench` was skipped because a `build-test` job failed (`needs: build-test`), for example a
flaky Windows build in the nightly run. Its first real step downloads `bench-linux`
(`ci.yml:335`); `actions/download-artifact@v4` fails when the artefact does not exist, so the job
turns red as a second, misleading failure. Worse, the step "Acceptance marker" comes after the
download: for a push that merges a pull request with `bench-accepted`, the marker is then never
written, the anchor does not move, and later runs of `main` fail with drift that was accepted. The
push step already handles "no sample in the artefact" and still writes the marker
(`ci.yml`, "Push to"); only the missing artefact defeats it.

## O5 - failure - minor

**A failed label lookup silently writes no acceptance marker.**

The marker step reads the labels with
`gh api …/commits/<sha>/pulls --jq '.[0].labels[].name' || true` (`ci.yml:346`). A failure of the
API call (rate limit, transient error) is swallowed and treated as "no label": no marker, no
warning. The consequence is the same as in O4, drift on later runs of `main` for a slowdown that was
accepted, and nothing in the log of the merge run points at the cause. The rule "Failures of the
mechanism" asks for a warning where the mechanism cannot do its part.

## O6 - operational - minor

**The run that accepts drift is itself red.**

Drift is resolved "by accepting it through the `workflow_dispatch` input". The dispatch on `main`
with `accept_drift` runs the bench job first, whose gate (`--kind main`) still sees the old anchor
and fails on the very drift being accepted (`gate.cpp:396-398`; `ci.yml:244-256`); only afterwards
does `bench-record` write the marker. The person accepting the drift gets a red run as confirmation
and has to know that red is the expected outcome here. The same holds for the push of a merge with
`bench-accepted` if drift already existed. Neither the story nor `docs/performance.md` says so.

## O7 - failure - minor

**A third run of main cancels the pending second one and its sample.**

`concurrency` keeps `group: ci-${{ github.ref }}` with `cancel-in-progress` false for runs of
`main` (`ci.yml:23-26`). GitHub allows one running and one pending run per group; a newly queued run
cancels the pending one even with `cancel-in-progress: false`. Three merges within one CI duration
(about 20 min), or two merges around the nightly run, cancel the middle run, and with it its sample
and, for a merge with `bench-accepted`, its acceptance marker (the record job is cancelled with the
run). The scenario only covers "a second merge shortly
after the first", which holds; the rule "runs of `main` finish" does not hold in general.

## O8 - threat - minor

**The record job does not check the test branch name itself.**

The only job with `contents: write` takes its target branch straight from the input
(`BRANCH: ${{ inputs.bench_data_branch != '' && inputs.bench_data_branch || 'bench-data' }}`,
`ci.yml:332`); its condition checks only that the input is not empty and the ref is not `main`
(`ci.yml:322-325`). The prefix `bench-data-test-` is validated in the bench job only
(`ci.yml`, step "Kind of run and data branch"). With an invalid name the bench job fails before any
file of the artefact exists, so the download fails and nothing is pushed; the protection holds only
because of that ordering. A later change that uploads something earlier, or makes the download
optional (a natural fix for O4), would let a dispatch push to any branch name, for example
`bench-data` from a feature branch.

## O9 - design - minor

**Drift listed in a pull request run includes the pull request's own results.**

The story lists drift in pull request runs "for information, since the pull request did not cause
it". `decide` computes drift for both kinds with the run's own results appended as the newest sample
(`gate.cpp:378-387`). In a pull request run those results are the pull request's, so a key it slows
down can appear as "drift of the class" while the class itself has not drifted. Information that
mixes the class and the pull request misleads exactly where the report claims the pull request is
not the cause.

## O10 - design - minor

**The unknown class gets samples, an anchor and a drift check across different hardware.**

`linux-unknown-…` "never becomes established" because runners without a model name may be
different hardware (`isEstablished`, `gate.cpp:231-233`). The record job still writes samples for it
(no check in the "Sample for bench-data" step), and `decide` computes an anchor and drift for any
class (`gate.cpp:378-387`), the unknown one included. A run of `main` on an unknown runner can
therefore fail on "drift" between unrelated machines, the very comparison the class scheme exists to
avoid.

## O11 - operational - minor

**Failures of the main stage itself reach the log but not the job summary.**

"If the base commit does not build, or a round writes no complete results, the job fails and says
so." For an incomplete round the gate prints the reason to stderr and exits before writing a
report (`hitokoto_bench_gate.cpp:372-381`); the step then appends a missing `gate-report-main.md`
with `|| true` (`ci.yml:280`). A base that does not build fails in `cmake --build ../base/…`
(`ci.yml:262-264`). In both cases the summary ends with "Main stage: base <sha>" and no reason; the
reader has to open the log. The summary is the place the story names first for what the job
reports.

## O12 - operational - minor

**Runs of main show SLOW in the class table next to a green outcome.**

For runs of `main` the class stage "is reported but never fails the job". The report uses the same
table as for pull requests, so keys above tolerance are marked `SLOW`, the word that means "fails"
everywhere else (`hitokoto_bench_gate.cpp:140-174`), while the outcome line says `green`. After an
accepted slowdown this is the normal state for up to five runs per class. The summary does not say
that the class stage is information only in runs of `main`.

## Capped concerns

- The report says "1 samples" (`hitokoto_bench_gate.cpp:185`), seen in dispatch 42575960727;
  plural for one sample.
- `docs/performance.md` (lines 89, 169, 208) still says that values "in `bench/baselines/linux.json`
  come from" certain runs, although the file is removed; as history it is correct, but it reads as
  current.
- A re-run of a run of `main` (attempt 2) adds a second sample of the same commit; two of the five
  samples in the window can then be the same code on possibly the same runner.

## Explicitly not objecting to

- **The sample of the run that writes a marker does not count for the new anchor.** Marker and
  sample carry the same `start_time`, and `anchor()` takes samples strictly newer than the marker.
  This matches the rule ("newer than the newest marker") and only delays the anchor by one sample.
- **`fetch-depth: 0` instead of the `2` the story names.** Dispatches on other branches need the
  merge base with `origin/main`, which a depth of 2 does not reach; the base of pull request runs is
  still `HEAD^1` as decided (review O3). The story text could say so.
- **`issues: read` in the bench job besides `pull-requests: read`.** The labels of a pull request
  are read through the issues endpoint; the permission is read-only.
- **Re-creating the commit on a rejected push instead of rebasing.** Each sample is a new file, so
  fetching the branch again, copying the file and committing is equivalent to a rebase and simpler.
- **`Qt6::Core` in `hitokoto-bench-gate`.** The decisions stay in the standard-library gate,
  unit-tested in `core-sanitize`; the executable only reads files and JSON, as ADR 0009 decides.
- **Timeout of 30 min.** Derived from the measured 7 min 52 s with room for slower classes, as the
  story asks.

## Disposition of the capped concerns

- **"1 samples":** resolved. The report no longer counts samples; the class is named for
  information only.
- **`docs/performance.md` names `bench/baselines/linux.json` as the source of values:** fixed with
  the rework of `docs/performance.md` for the revised S029; the history says that the file was
  removed with S029.
- **A re-run of a run of main adds a second sample:** resolved. Runs of main add no samples.
