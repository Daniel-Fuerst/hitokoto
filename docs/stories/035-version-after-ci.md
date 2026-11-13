# S035 — A version only after the CI on main is green

- **Status:** Done (accepted by Armin, 2026-11-12, after the merge of pull request 42 with
  `version:none`: the CI on main ran "Version" as its last job without a new tag, and "Packaging"
  after it published 0.5.1 to the AUR; merged without the step In review, because the chain runs
  only on main); Confirmed (Armin approved the design in conversation, 2026-11-12; review
  `docs/reviews/035-version-after-ci-spec.md`) · **Order:** 36 ·
  **Size:** S · **Depends on:** S030, S034 · **Decision:** ADR 0010 (amended)
- **Basis:** On a push to main, "Version" (S030) and "CI" start side by side, and "Packaging"
  (S034) follows "Version". So a merge gets its version tag and its AUR update while its CI is still
  running, also when that CI ends red (Armin, 2026-11-12, after the merge of pull request 40). The
  same day the first publish of S034 failed on the hand-over of the files between its jobs (run
  43292098955, `docs/reviews/034-aur-package-code.md` has the earlier record); that fix goes into
  the same pull request. Armin also asked that an unreachable AUR makes neither the CI nor a pull
  request red.

## Story

As the maintainers, we want a version tag, and everything built from it, only for a merge whose CI
on main is green, so that no version is ever published that the CI has not passed, benchmarks
included.

## Decisions (approved by Armin in conversation, 2026-11-12)

1. **"Version" runs inside the CI run.** `ci.yml` gets a last job "Version" that calls `version.yml`
   (`workflow_call`) on a push to main, after every other job of the CI (`needs`), benchmarks
   included. It runs unless one of them failed or was cancelled; a skipped job does not stop it
   (review O1). `version.yml` keeps the label check on pull requests and loses its own trigger on a
   push to main and its manual trigger: tags come only from a green CI run. The tag job checks out
   the commit of that CI run and tags merges up to it, never a later commit.
2. **A red CI on main gives no version until it is green.** A job that failed by chance (a
   benchmark outlier) is started again with "Re-run failed jobs"; once the run is green, "Version"
   runs and tags. A merge whose CI stays red keeps no tag; the tag job of the next green run tags
   every untagged merge before it (S030), each with its own CI having run.
3. **CI runs on main are never cancelled.** Today a new push cancels a running CI run of the same
   ref; on main that would drop a "Version" run. The concurrency group of `ci.yml` keeps cancelling
   for pull requests and gives every push to main its own group.
4. **"Packaging" follows the CI directly.** `packaging.yml` runs after a completed, successful CI
   run on a push to main (`workflow_run` on "CI" instead of "Version"). The CI run belongs to the
   merge commit, so `github.event.workflow_run.head_sha` is exactly the commit the CI passed, and
   its version tag already exists. Only one `workflow_run` step: a workflow started by
   `workflow_run` belongs to the newest commit of main, so a second step would lose the commit.
   Debian and Windows packages (later stories) go into `packaging.yml` the same way.
5. **The AUR does not decide the colour of the CI.** "Packaging" stays a workflow of its own: if the
   AUR is unreachable, only "Packaging" is red (with GitHub's e-mail) and can be started again on
   its own. In a dry run (pull request, manual run without "publish") an unreachable AUR gives a
   warning; the package is still built, only the comparison with the AUR is left out.
6. **Hand-over of the files (S034, fix).** The build job writes the three files for the AUR into the
   workspace and uploads that directory; in the container job, `${{ runner.temp }}` does not name
   the directory `$RUNNER_TEMP` names. A job without the environment checks the artifact before the
   publish job uses it, also in a dry run.
7. **Lints keep the wiring.** `ctest` cases `S035_*` check the workflow files: the "Version" job of
   `ci.yml` needs every other job of the CI, `version.yml` has no trigger of its own on main,
   `packaging.yml` follows "CI". The lint also checks that "Version" runs unless a needed job
   failed or was cancelled.

## Acceptance scenarios

**Scenario: a version only after a green CI**
- Given a pull request labelled `version:patch` is merged
- When the CI run on the merge commit is green, with all jobs including the benchmarks
- Then its last job "Version" tags the merge commit, and after the run "Packaging" starts with that
  commit and publishes the new version to the AUR

**Scenario: a red CI gives no version**
- Given the CI run on a merge commit fails in any job
- Then "Version" does not run, no tag is set and "Packaging" does not run
- And after "Re-run failed jobs" turns the run green, the tag is set and "Packaging" runs

**Scenario: a second merge during the CI**
- Given merge y lands on main while the CI on merge x runs
- Then the CI on x is not cancelled, its "Version" tags x if x has no tag yet, never y, and its
  "Packaging" uses x
- And the CI on y tags y and starts its own "Packaging" with y

**Scenario: an unreachable AUR**
- Given the AUR cannot be reached
- When "Packaging" runs after a green CI on main, it is red, the CI stays green, and "Packaging" can
  be started again on its own
- When it runs as a dry run on a pull request, it warns, builds the package and stays green

**Scenario: the files reach the publish job**
- Given the AUR is behind the highest version tag
- Then the job "Check the hand-over" finds exactly `PKGBUILD`, `.SRCINFO` and `hitokoto.desktop` in
  the artifact, also in a dry run, and the publish job pushes them

**Scenario: the wiring is checked**
- Then the `ctest` cases `S035_*` fail if a job of `ci.yml` is missing from the needs of "Version",
  if `version.yml` gets a trigger on a push to main, or if `packaging.yml` no longer follows "CI"

## Notes

- If the CI on a later merge y finishes before the CI on an earlier merge x, the tag job of y tags
  x as well (S030 catch-up): y contains x, and its CI passed with x in it (review O2).
- A CI on main that stays red, also a benchmark that fails for good, is fixed by a pull request like
  any red CI; the first green run tags the merges in between (review O3).
- That GitHub sends `workflow_run` again when a re-run attempt of the CI completes is assumed; it
  is checked at the first re-run. If not, "Packaging" is started by hand on main (review O4).
- `workflow_run` runs the workflow file of the newest commit of main, not of the commit it got;
  only the checked-out files come from that commit. For `packaging.yml` this is accepted.
- The first run of the new chain is the merge of this story's pull request (`version:none`): its CI
  runs "Version" (no tag), and "Packaging" then catches up `v0.5.1` in the AUR.
