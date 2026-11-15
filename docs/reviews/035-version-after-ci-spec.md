---
mode: spec
spec: docs/stories/035-version-after-ci.md
reviewed: 2026-11-12
reviewer: Claude (Advocatus Diaboli, same session)
objections:
  - id: O1
    category: failure
    severity: major
    title: A skipped CI job silently stops every version
    disposition: fixed
    disposition_rationale: >-
      Decision 1: "Version" runs unless a needed job failed or was cancelled; a skipped job does
      not block it. Lint S035_TheVersionJobNeedsEveryCiJob also checks that condition.
  - id: O2
    category: failure
    severity: minor
    title: A catch-up tags an earlier merge before its own CI is done
    disposition: documented
    disposition_rationale: >-
      Decision 2 and the notes: the later merge contains the earlier one and its CI passed; a tag
      on the earlier merge follows that run. Checking every earlier merge's own CI run would need
      the Actions API in the tag job; not worth it for two maintainers.
  - id: O3
    category: operational
    severity: minor
    title: A benchmark that fails for good blocks every version
    disposition: documented
    disposition_rationale: >-
      Notes: a lasting red benchmark on main (for example a new runner class) is fixed by a pull
      request like any red CI; the versions of the merges in between are caught up with it.
  - id: O4
    category: operational
    severity: minor
    title: That a re-run starts "Packaging" again is assumed, not shown
    disposition: documented
    disposition_rationale: >-
      Notes: GitHub sends workflow_run again when a re-run attempt completes; to be confirmed at
      the first re-run, and "Packaging" can be started by hand otherwise.
---

# S035 — A version only after the CI on main is green: objections (spec)

Story as drafted on 2026-11-12 on `packaging/aur-update-on-new-version` (main bd7d57a), read
together with `ci.yml`, `version.yml` and `packaging.yml` on main and ADR 0010.

## O1 - failure - major

**A skipped CI job silently stops every version.** Decision 1 makes "Version" need every other job
of the CI. By default a job runs only if all its needs succeeded; a needed job that is *skipped*
(for example a future job with `if: github.event_name == 'pull_request'`, or one that is turned
off by a condition) skips "Version" as well. The run is then green, no tag is set, and nothing
says why.
*Wanted:* "Version" runs unless a needed job failed or was cancelled
(`always()` with a check of `needs.*.result`), and the lint of decision 7 checks that condition.

**Disposition: fixed.** Decision 1 says so; the lint checks it.

## O2 - failure - minor

**A catch-up tags an earlier merge before its own CI is done.** The tag job of S030 tags every
untagged merge up to its commit. With CI runs on main no longer cancelled (decision 3), the run on
a later merge y can finish before the run on an earlier merge x and tag x as well, while x's own
CI is still running or later ends red. The scenario "a second merge during the CI" claims that the
run on x tags x.
*Wanted:* say which run tags what in that case, or let the tag job check each earlier merge's own
CI run.

**Disposition: documented.** y contains x, and y's CI passed with x in it; a tag on x then follows
from that run. The scenario says "tags x, if not yet tagged"; the notes name the case. Checking
each earlier merge's run would need the Actions API in the tag job, not worth it for two
maintainers merging a few times a day.

## O3 - operational - minor

**A benchmark that fails for good blocks every version.** "Re-run failed jobs" helps against an
outlier, not against a lasting red benchmark on main (a new runner class without an anchor, a real
regression that the pull request gate let through with `bench-accepted`). Until it is fixed, no
merge gets a version.
*Wanted:* name the way out.

**Disposition: documented.** The notes say that a lasting red CI on main is fixed by a pull request
like any other red CI, and that the versions of the merges in between are caught up with the first
green run (S030 catch-up).

## O4 - operational - minor

**That a re-run starts "Packaging" again is assumed, not shown.** Decision 2 and the scenario "a
red CI gives no version" rely on GitHub sending `workflow_run` again when a re-run attempt of the
CI completes.
*Wanted:* name it as an assumption to check at the first re-run, and the way out if it does not
hold.

**Disposition: documented.** The notes say so; without it, "Packaging" is started by hand on main
(it catches up).

## Explicitly not objecting to

- "Version" inside the CI run and "Packaging" one `workflow_run` step behind it (decisions 1 and 4):
  it gives "Packaging" the commit the CI passed.
- Benchmarks as part of the condition (decision 2): the pull request gate already compares with
  main; a green main run is the stricter rule Armin chose.
- A concurrency group per push on main (decision 3); the tag job keeps its own group.
- The AUR outside the CI's colour, with a warning in dry runs (decision 5).
- Lints over the workflow files (decision 7), in the style of the existing lint tests.
