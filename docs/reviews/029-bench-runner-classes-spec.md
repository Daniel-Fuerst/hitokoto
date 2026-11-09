---
story: S029
mode: spec
spec: docs/stories/029-bench-runner-classes.md
related: docs/adr/0009-benchmark-baselines-per-runner-class.md
date: 2026-11-08
dispositioned: 2026-11-08 (Armin decided O2, the anchor per class; the others and the capped concerns as proposed by Claude, before confirmation of the story)
reviewer: Advocatus Diaboli (independent subagent)
objections:
  - id: O1
    category: premise
    severity: major
    title: "Runs on one runner already differ by more than the tolerance, so the main stage can still be red without a code change"
    disposition: fixed
    disposition_rationale: "Checked on the artefacts: in the three class B runs of PR #24 (same commit) 2 of 39 compared keys swing by more than 25 % and 2 ms (s023_open_10mb_first_chunk_ms 54/54/74 ms, s024_open_10mb_properties_ms 31/56/61 ms), the same two in the class A runs. Story: Background corrected; the main stage measures four fresh rounds and fails a key only if the means differ beyond tolerance and the rounds of both sides do not overlap; scenario 'an opening that jumps between two levels'. The development-VM case in performance.md is not a CI runner and not used as evidence."
  - id: O2
    category: premise
    severity: major
    title: "Baselines that follow main automatically allow slow regressions to add up with no limit"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-08: anchor per class. Story decision 7 and rules 'Anchor and drift': the anchor is the median of the oldest 3 samples after the newest acceptance marker; markers come from merges of pull requests with bench-accepted (REST lookup) and from a workflow_dispatch input on main; a run of main whose class baseline drifted more than 25 % and 2 ms from the anchor fails; pull request runs list drift for information. Scenarios 'drift on main' and 'an accepted slowdown moves the anchor'; ADR 0009 decision and consequences."
  - id: O3
    category: design
    severity: major
    title: "pull_request.base.sha is not reliably the first parent of the merge commit CI tests"
    disposition: fixed
    disposition_rationale: "Story, main stage: the base is HEAD^1 of GITHUB_SHA, fetched with fetch-depth 2, and the summary names the base commit it measured."
  - id: O4
    category: design
    severity: minor
    title: "The class key leaves out cache sizes, and it is unverified that model names separate classes A to D"
    disposition: fixed
    disposition_rationale: "Story, rules 'Runner class': the class is model name, CPU count (nproc) and the L2 and L3 sizes from /sys/devices/system/cpu/cpu0/cache; the caches separated A–D in the artefacts, so they keep classes apart even if a hypervisor reports one model name for different hardware."
  - id: O5
    category: design
    severity: minor
    title: "The A/B rounds are uneven (base measured right after a build, the PR round chosen because it was slow)"
    disposition: fixed
    disposition_rationale: "Story, main stage: four fresh rounds after the build in the order base, pull request, pull request, base; the run of step 1 counts only for hard budgets and the class stage."
  - id: O6
    category: design
    severity: minor
    title: "The main stage cannot see slowdowns from the toolchain or Qt, because both sides use the PR's toolchain"
    disposition: fixed
    disposition_rationale: "Story, main stage: if the pull request changes the toolchain (Qt version or other steps of ci.yml, CMakePresets.json, cmake/), the log says both sides were built with it, and a failure of the class stage stands unless the label is set; scenario 'the toolchain changes'. Building the base with the base's Qt is not done: it would double the Qt installation."
  - id: O7
    category: design
    severity: minor
    title: "No preset or CI job builds and runs the evaluating-mode ctest cases, and the workflow/evaluator interface is not specified"
    disposition: fixed
    disposition_rationale: "Story, requirements: the gate library in bench/gate/ is built whenever tests are built (also core-sanitize); the deciding moves to a small executable hitokoto-bench-gate (gate library plus Qt6::Core, no QApplication), built in the dev, ci and bench presets, with its ctest cases in the build-test jobs; exit codes 0 green, 1 fails, 3 run the main stage, plus a report for log and summary."
  - id: O8
    category: failure
    severity: minor
    title: "The spec is inconsistent about unreadable samples; one bad file switches off the class stage for good, and the writer does not validate"
    disposition: fixed
    disposition_rationale: "Story: an unreadable sample is skipped and counted as missing, with a warning naming the file (rules, scenario 'an unreadable sample', tests); the writer records only complete results (file parses, context fields, every hard-budget key present)."
  - id: O9
    category: failure
    severity: minor
    title: "Exit codes and hard budgets of the extra main-stage rounds are not specified"
    disposition: fixed
    disposition_rationale: "Story, main stage: a round counts if hitokoto-bench wrote complete results, whatever its exit code; hard budgets of the pull request's rounds fail the job, those of the base's rounds are information."
  - id: O10
    category: operational
    severity: major
    title: "The workflow-level concurrency cancels runs of main, so 'never cancelled' sample recording is not possible as specified"
    disposition: fixed
    disposition_rationale: "Story, rules 'Cancelling' and requirements: cancel-in-progress only for pull_request; the job that adds samples has no concurrency group of its own (one new file per sample, rebase and retry). Scenario 'a run of main adds a sample' covers a second merge shortly after the first. Part of the ci.yml changes that need Armin's approval."
  - id: O11
    category: operational
    severity: major
    title: "The CI verification through workflow_dispatch contradicts the story's own permission rules and depends on chance"
    disposition: fixed
    disposition_rationale: "Story, rules 'Test input': the one exception to 'only runs of main write' is stated and limited (branch name bench-data-test-…, not on main, dispatch only). CI verification reduced to writing and reading back a sample; the established class is covered by the ctest cases; scenario 'after the merge of this story' checks the first push and nightly run by hand before Done."
  - id: O12
    category: cost
    severity: minor
    title: "The 'at most doubles the measuring part' budget is wrong arithmetically (four e2e runs instead of one)"
    disposition: fixed
    disposition_rationale: "Story, requirements: one build and four more runs of hitokoto-bench, the measuring part five times today's in those runs; no core benchmarks for the base; duration measured on the story's pull request, timeout-minutes derived from it. ADR 0009 option 4 corrected."
---

# Spec review S029 — Benchmark baselines per runner class

Objections as delivered by the independent reviewer (2026-11-08), on the first version of the story
(748e7e7). Dispositions in the front matter; the capped concerns are dispositioned at the end.

## O1 - premise - major

**Runs on one runner already differ by more than the tolerance, so the main stage can still be red without a code change.**

The story assumes that "Within a class the runs of one commit agree within a few per cent" (story line 33). The only exception it names is the decoding outlier of PR #26. The data this story is built on contains more exceptions than that:

- `docs/performance.md` lines 159–160 (three runs of one commit on class B): "The three ordinary runs agree within a few per cent, except `s024_open_10mb_properties_ms` (31, 55 and 61 ms; 49 ms in the first run)." The removed `unrefined.md` entry (`git show 748e7e7`) also says "except `s024_open_10mb_properties_ms` (31, 55, 61 ms) and single maxima". The story's Background leaves this out. The key is compared: it is not in `kReportOnly` (`bench/hitokoto_bench.cpp` lines 763–799), and its baseline value is 60.7 ms (`bench/baselines/linux.json` line 48).
- `docs/performance.md` lines 213–215 (one machine, main against main): "against one run of main (tolerance 25 %), two further runs of main itself failed the comparison (table typing p95 6.5 → 9.9 and 10.6 ms, save stall 4.1 → 6.6 ms)". Both keys are compared keys. Both moves exceed 25 % and 2 ms.

The main stage takes the lower of two rounds per side (story lines 92–94) across 49 compared keys (86 baseline keys minus 37 report-only, counted from `linux.json` and `kReportOnly`). With the spread above, "base 31/55, PR 55/61" gives 31 against 55 ms, which is +77 % and +24 ms: red. The main stage is the final judge, so a red result there brings back "restart and hope", which the story exists to remove. ADR 0009's "Reconsider if: Two runs of one commit on one runner differ by more than the tolerance" (ADR line 85) is already met by evidence in the repository.

**What would resolve it:**

- Before confirming, compute the per-key spread on one runner from the 17 kept artefacts and from the runs of PR #24.
- Estimate the false-red rate of min-of-2 against min-of-2 over the 49 keys.
- Then choose one of these:
    - more rounds or a median of rounds;
    - re-measure only the failing keys before failing;
    - a warm-up drop (option 4 of the old entry);
    - make `s024_open_10mb_properties_ms` and similar keys report-only.
- Correct the Background statement.

## O2 - premise - major

**Baselines that follow main automatically allow slow regressions to add up with no limit.**

Today, changing a baseline is a deliberate human act: "Only update baselines on purpose (a known, accepted change), never to make a red build green" (`bench/baselines/README.md` line 73). Under S029, every merged commit feeds its class baseline: "the class baseline follows the new code" (story lines 195–199; ADR lines 73–74). The main stage only ever compares with the immediate base. So:

- A series of pull requests, each up to 24 % slower on a key, is green every time.
- No person is ever told about the total drift.
- After a few such merges, the baseline itself sits at the slower values.

The brief's soft budgets (10 MB open < 3 s, startup < 1000 ms) do not stop this. The job fails only on hard budgets unless `--fail-on-budget` is passed (`bench/hitokoto_bench.cpp` lines 1116–1121), and `ci.yml` line 161 does not pass it. `open_10mb_styled_ms` is 2.6 s on the baseline (`linux.json`) and can drift past the brief's 3 s with no red run. The brief asks to "fail or flag the build on significant regressions" (section 4). A gradual regression is still a significant one.

**What would resolve it:**

- Keep an anchored reference per class that moves only on purpose (for example, with a `bench-accepted` merge or by hand).
- Report in the summary the drift of the class baseline against that anchor, and flag or fail beyond a threshold.
- At the least, state the trade-off explicitly in ADR 0009 as an accepted consequence, and give the README rule a successor.

## O3 - design - major

**`pull_request.base.sha` is not reliably the first parent of the merge commit CI tests.**

Rule 3 (story lines 89–91) builds "`pull_request.base.sha`, the first parent of the merge commit CI tests". GitHub does not guarantee that these are the same:

- The payload's `base.sha` is not refreshed on every update. GitHub community discussion #59677, "github.event.pull_request.base.sha should be updated each time the pull request is updated" (https://github.com/orgs/community/discussions/59677), says the value "is not updated on every commit to the PR … becomes out of date as the PR gets updated". Its author parses the merge-commit message to get the real base.
- The tested commit (`GITHUB_SHA`, `refs/pull/N/merge`) is merged against the current tip of main.

This project has long-lived branches while main moves on: `feature/smooth_scrolling`, PR #26, merged main in commit c172780. In such a pull request the arbiter compares "PR plus all newer main commits" with old main. Main's own regressions or improvements are then blamed on the pull request, in both directions.

`actions/checkout@v4` with the default `fetch-depth: 1` (`ci.yml` line 133) also fetches neither parent. The spec does not say how the base gets fetched.

**What would resolve it:**

- Define the base as `HEAD^1` of the checked-out `GITHUB_SHA`. This holds by construction, and it is also stable across re-runs.
- Specify fetching it (`fetch-depth: 2` or an explicit `git fetch`).
- Log the base SHA in the summary.

## O4 - design - minor

**The class key leaves out cache sizes, and it is unverified that model names separate classes A to D.**

Classes A to D were told apart by clock and `context.caches` (story lines 23–31). The old entry states: "The artefact does not yet name the CPU model". Its proposed first step was to "record the CPU in the artefact and collect a few runs to learn the classes". That step has not happened, yet the key is defined by `model name` alone (story lines 56–62).

The reason given for leaving out the clock is that it varies within one model. That reason does not apply to L1d/L2/L3 sizes, which are fixed per model. If a hypervisor reports a generic or shared model string for different hardware, the median mixes classes:

- For a fast runner, the class stage then passes against a slow mixed baseline (a false green that never reaches the arbiter).
- For a slow runner, it sends every pull request to the main stage.

**What would resolve it:**

- Include L2/L3 sizes in the class key.
- Or record the model name for a few runs first and confirm that it splits A to D.
- And flag in the summary a sample whose caches differ from the others in its class.

## O5 - design - minor

**The A/B rounds are uneven.**

The rounds are "pull request (the run of step 1), base, pull request, base" (story lines 92–93), and the base is built between PR round 1 and base round 1. This creates two biases that are not measured:

1. **Base round 1 runs straight after a full Release build** of app, UI and benchmarks on the same 4 vCPUs. PR round 1 ran before that build, after the core benchmarks. Turbo, thermal and page-cache state differ by side.
2. **PR round 1 is selected by its own slowness.** The main stage runs only because step 1 exceeded the class baseline. That round is therefore biased high, so the PR side in effect has one useful round (PR round 2) against the base's min-of-two.

The first bias favours the pull request; the second works against it. Neither is quantified, and for keys near 25 % they decide the result.

**What would resolve it:**

- Once the main stage starts, measure fresh, balanced rounds after the build (for example, base, PR, base, PR, or ABBA with a discarded warm-up run).
- Use the step-1 run only for hard budgets.
- Or justify the current order with data from the CI run of this story.

## O6 - design - minor

**The main stage cannot see slowdowns from the toolchain or Qt.**

The base is built in the pull request's job with the pull request's Qt (`install-qt-action`, `ci.yml` lines 140–145, version from the matrix at line 126), compiler and apt packages. Only the CMake tree is the base's own. Consider a pull request that bumps the Qt version, which the project does deliberately (ADR 0001; `ci.yml` lines 14–16) and which makes everything 30 % slower:

- The class stage fails, because the class baseline was built with the old Qt.
- The main stage then compares new Qt with new Qt and is green.

The spec logs harness changes under `bench/` (story lines 95–96) but not changes to the toolchain.

**What would resolve it:**

- When the pull request changes the Qt version, `ci.yml` toolchain steps or presets, the log should say so, as for `bench/`.
- Better: in that case the class-stage result needs the label to pass.
- Alternatively, install and use the base's Qt version for the base build.

## O7 - design - minor

**No preset or CI job builds and runs the evaluating-mode ctest cases, and the workflow/evaluator interface is not specified.**

The Tests section requires "`ctest` cases that run `hitokoto-bench` in its evaluating mode on fixed files" (story lines 231–233). `hitokoto-bench` exists only with `HITOKOTO_BUILD_BENCHMARKS=ON` (`CMakeLists.txt` lines 46–48). No preset combines benchmarks with tests (`CMakePresets.json`):

| Preset | Benchmarks | Tests |
|---|---|---|
| `dev` | off | on |
| `ci` | off | on |
| `bench` | on | off |

The only job that turns benchmarks on with tests is `tidy` (`ci.yml` line 106), and it runs no `ctest`. As written, these tests would never run in CI.

ADR 0009 says the gate library is built "whenever tests are built, also in core-sanitize". That too needs a new `add_subdirectory` outside the benchmarks condition, which the spec does not mention.

`main()` creates a `QApplication` and writes the 10 MB documents before anything else (`bench/hitokoto_bench.cpp` lines 853–890). The evaluating mode has to branch before the document writes.

The workflow also has to act on a three-way result: green, red, or "run the main stage". The existing exit codes are 0 and 1 only. Without a defined contract, decision logic leaks into bash, against "the workflow only orchestrates".

**What would resolve it:**

- Name where these tests are built and run (for example, `ctest` in the bench job, or a preset with both options). This is a `ci.yml` or preset change and needs approval.
- Specify the exit-code or output-file contract of the evaluating mode.

## O8 - failure - minor

**The spec is inconsistent about unreadable samples.**

Three places give different answers:

- **Rules, lines 123–124:** an unreadable sample means "the job warns and treats the class as not established (main stage)".
- **Tests, line 230:** "unreadable samples treated as missing".
- **Scenario, lines 190–193:** an unreadable sample means "main stage decides".

Under the Rules version, one bad file stays on `bench-data` and switches off the class stage for that class permanently. Every pull request on it runs the main stage, and the only signal is a warning. `bench/baselines/README.md` line 61 itself observes that "a comparison that only reports is read by nobody".

Nothing stops such a file being written. Samples are recorded even when hard budgets fail (story lines 66–67), and nothing requires the writer to check that `e2e-bench.json` exists, parses and is complete, for example after a harness crash.

**What would resolve it:**

- Choose one semantics: skip the file and count it as missing, with a warning naming it.
- Require the writing step to validate the JSON (parses, has the expected context fields, has a minimum key set) before pushing.
- Never record a sample from a run whose harness did not finish.

## O9 - failure - minor

**Exit codes and hard budgets of the extra main-stage rounds are not specified.**

`hitokoto-bench` exits 1 on any hard-budget miss, with no option to turn that off (`bench/hitokoto_bench.cpp` lines 1119–1121). If a base round hits a scheduler hiccup over 50 ms, which has happened (the old entry cites PR #20 at 222 ms and PR #24 at 54.2 ms), its exit code is 1. The spec says "If the base commit does not build or its benchmark does not run, the job fails" (story lines 125–126), so a hiccup in main's own run would fail the pull request.

For PR round 2 the spec contradicts itself:

- Step 1 checks only "the pull request's run" (line 82).
- The scenario says hard budgets fail "in any stage" (lines 185–188).

**What would resolve it:**

- Define "runs" as "exits with complete JSON", so that exit code 1 with full results counts as a run.
- State that base rounds' hard budgets are for information only.
- Decide whether hard budgets of PR round 2 fail the job or are for information.

## O10 - operational - major

**The workflow-level concurrency cancels runs of main, so "never cancelled" sample recording is not possible as specified.**

`ci.yml` lines 10–12 set `group: ci-${{ github.ref }}` with `cancel-in-progress: true` at workflow level. Push, schedule and `workflow_dispatch` on main all have `github.ref == refs/heads/main`. So:

- A second merge cancels the first merge's run.
- A push cancels a running nightly run, and the nightly cancels an in-progress push run.

The bench job waits for the whole four-OS `build-test` matrix (`needs: build-test`, line 118), so the window for cancellation is long. Cancelling the run cancels every job in it, including a sample job with its own group. The requirement "serialised by its own concurrency group, never cancelled" (story lines 214–215) therefore cannot hold without changing the workflow-level block.

A job-level group is not a queue by default either. GitHub's documentation (https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency) says that with the default queue (`single`), "any existing pending job or workflow in the same concurrency group will be canceled" when a new one is queued. `queue: max` exists, but "Combining `queue: max` with `cancel-in-progress: true` is not allowed".

**What would resolve it:**

- Specify the `ci.yml` change explicitly, for example `cancel-in-progress: ${{ github.event_name == 'pull_request' }}`, or a group per SHA on main. This is a change beyond the bench job and needs approval.
- For the sample job, either use `queue: max` without cancel-in-progress, or drop the group and rely only on the rebase-and-retry. One file per sample means pushes never conflict.

## O11 - operational - major

**The CI verification through `workflow_dispatch` contradicts the story's own rules and depends on chance.**

The test plan wants a `workflow_dispatch` on the story branch, with a test target branch, where "samples added, then a run that finds its class established" (story lines 234–237). The Rules say otherwise:

- "Runs on other branches through `workflow_dispatch` behave like pull request runs" (lines 109–111), which add nothing and have no write access.
- `contents: write` is allowed only for "push, schedule and workflow_dispatch on main" (lines 212–214).

The test path therefore needs a write-enabled route from a non-main branch. That is an unstated exception to the permission model and to the scenario "pull request runs add nothing … no write access" (lines 149–152).

"A run that finds its class established" needs three samples of one class and then a fourth run that lands on that class. Runner classes are random (B 7/17, A 6/17, C and D 2/17 each). Each attempt is a full workflow run, including the four-OS build matrix. The number of runs this takes is open-ended.

The push and schedule paths run the workflow from the default branch, so they cannot be exercised before the merge at all.

**What would resolve it:**

- Either state and limit the permission exception explicitly (input-gated, test branch only, refused on main), or test only the writing mechanism in CI and leave "established class" to the ctest fixtures.
- Add post-merge checks to acceptance ("first main run adds a sample"; "first nightly run adds a sample").

## O12 - cost - minor

**The "at most doubles the measuring part" budget is wrong arithmetically.**

The main stage runs PR (step 1), base, PR, base (story lines 92–93): three e2e runs and one build on top of the one run done today. The requirement nevertheless says "The main stage at most doubles the measuring part of the job plus one build" (line 218), and ADR option 4 says "doubles the measuring time". By its own definition the e2e part grows fourfold. The spec also does not say whether the core benchmarks run for the base.

The job has no `timeout-minutes`, and until classes are established every pull request pays this cost (Migration, lines 129–130).

**What would resolve it:**

- Correct the budget to "four e2e runs plus one build", or reduce the rounds (in tension with O1 and O5).
- State whether core benchmarks run for the base.
- Set a job timeout derived from the measured duration.

## Capped concerns

- Hard-budget hiccups (PR #20 at 222 ms; PR #24 class D at 54.2 ms) remain a cause of red runs without a code change. The story's goal "red only for a real regression" overstates what it achieves, and the spec does not name this residual.
- The `bench-accepted` label persists across later pushes to the same pull request and silently accepts further, unrelated regressions. It could be bound to the head SHA, or the summary could show which commit it was given for.
- "Only CI writes" to `bench-data` is not enforced: there is no ruleset, and a same-repo pull request can edit `ci.yml` to request `contents: write`. Samples are trusted inputs; the median of 5 tolerates only 2 bad ones.
- The sample window is ordered by "UTC time", but the source of that time is unspecified (run `created_at`, `run_started_at`, or job time). Re-runs of an old main commit enter the window as "newest", and repeated runs of one commit (re-runs, nightly runs with no merges) can fill a class alone.
- The nightly run executes the whole workflow (fuzzing, tidy, four-OS builds) for one Linux sample, and a Windows or macOS failure in `build-test` costs the sample (`needs: build-test`).
- The source of the CPU count is unspecified (`nproc`, cgroup limits, or `processor` lines in `/proc/cpuinfo`).

## Explicitly not objecting to

- **Storing samples on an orphan `bench-data` branch** rather than on main, in artefacts or cache, or in a separate repository. Pushes made with `GITHUB_TOKEN` start no workflow runs in any case, so the reasoning holds doubly. Size is negligible (about 3.6 KB per sample, judging by `linux.json`).
- **Gate logic in a standard-library module `bench/gate/`** instead of `src/core`, with Qt kept in `hitokoto-bench` for file and JSON work. This adds no dependency, uses one language, and respects the core/UI layering.
- **Pull request and base measured by different harnesses.** The harness links `hitokoto::ui` in-process, so the code and its harness cannot be separated. Logging changes under `bench/`, with the label as the escape hatch, is proportionate.
- **Failing safe:** missing or unfetchable data leads to the main stage and never to a pass without comparison. Main runs report the class stage but never fail on it.
- **Reading the label through the REST API at run time.** The default `pull_request` types do not include `labeled`, and a re-run reuses the original payload, so "add label, re-run" works as stated.
- **Fork pull requests:** they get a read-only token, and `bench-data` can still be fetched from the base repository through the checkout's `origin`.
- **Linux only** for now, with Windows and macOS left off.

## Disposition of the capped concerns

- Hard-budget hiccups: documented. The story's goal is about the comparison; hard budgets are
  absolute limits from the brief and stay as they are (a single key event over 50 ms is a real miss
  on that machine, even if rare). A later story may print the position of such samples (option in
  the removed `unrefined.md` entry).
- Label across later pushes: documented. The label applies to every run while it is set; the
  summary names the commit each run measured, and whoever merges sees which keys were accepted.
  Binding it to a head commit would need a second mechanism for the common case of a fix-up push.
- Writes to `bench-data` not enforced: documented in ADR 0009 (consequences: a ruleset could enforce
  it later). Only collaborators can push branches whose workflow changes run with write access, and
  the anchor (O2) makes a falsified baseline show up as drift.
- Source of the time: fixed. Samples carry and are ordered by `run_started_at`. Re-runs of an old
  commit and nightly runs without merges add samples of the same code; they are as valid a measure
  of that class as any other run of `main`.
- Nightly run executes the whole workflow: fixed in the story (rules "Nightly run": accepted as a
  nightly check of `main`; a failure elsewhere costs that night's sample).
- Source of the CPU count: fixed. `nproc` (the CPUs the job may use).
