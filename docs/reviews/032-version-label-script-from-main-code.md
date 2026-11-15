---
story: S032
title: "Objection record: S032 the check Version label takes its script from main (code)"
mode: code
spec: docs/stories/032-version-label-script-from-main.md
branch: agents/s032-version-label-from-main
commits_examined: "ff40964..e71cbc0 (1e0091a version.yml, e71cbc0 documentation); code: .github/workflows/version.yml"
date: 2026-11-10
reviewer: Advocatus Diaboli (Claude, in the session of the implementation)
objections:
  - id: O1
    category: design
    severity: major
    title: "A pull request that moves the script or changes how it is called cannot pass its own check"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-10: such a change is a deliberate rebuild of the versioning, and two pull requests for it are acceptable, as long as it is written down. Story S032, decision 2, and README, 'Versions': moving NextVersion.cmake or changing how it is called takes two pull requests, first the new interface beside the old one, then, after its merge, the switch in version.yml."
  - id: O2
    category: design
    severity: minor
    title: "An accidental change of version.yml itself still passes unnoticed"
    disposition: fixed
    disposition_rationale: "Armin, 2026-11-10: test-first ctest cases. S032_TheLabelCheckUsesMainsScript runs tests/version/s032_workflow_lint.cmake on version.yml (no pull_request_target outside comments; the job label checks out main once, with persist-credentials false, and runs NextVersion.cmake in the mode labels); S032_LintFinds* write a copy with one deviation each and expect the lint to name it (red with a stub, green with the lint). A deliberate change adapts the lint with it (story requirement)."
  - id: O3
    category: failure
    severity: minor
    title: "A green check is not refreshed when main's rules change before the merge"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-10: a ruleset rule 'require branches to be up to date' or a workflow that re-checks open pull requests on every push to main costs more than this rare case is worth. Story S032, decision 3: a pull request keeps the result of its last run; changed rules apply to open pull requests from their next event on (a push, a label, a re-run of Version label); until then the tag job stops after the merge."
  - id: O4
    category: operational
    severity: minor
    title: "The temporary commit of the verification turns the S030 cases in ci.yml red"
    disposition: resolved
    disposition_rationale: "Armin, 2026-11-10: the verification ran once and is done. On pull request 36 the temporary commit 76a560c turned the build jobs of CI run 147 red, as expected; it was reverted right after the check had shown its result (e92a9b7). The general point, expensive jobs that build fully before a cheap failure shows, became the entry 'CI: fail fast on cheap checks' in unrefined.md."
---

# Objection record: S032 the check Version label takes its script from main (code)

Mode `code`. Examined: the job "Version label" in `.github/workflows/version.yml` (1e0091a) and the
documentation (e71cbc0: requirement "Workflow" of S030, README "Versions", story status), against
the story and its decisions. Evidence beside the code: actionlint 1.7.12 with shellcheck 0.11.0
without findings; `action.yml` of `actions/checkout` v7 (inputs `ref`, `sparse-checkout`,
`sparse-checkout-cone-mode`, `persist-credentials`); a local stand-in for the step: a depth-1 clone
of main with `git sparse-checkout set --no-cone /scripts/version/` holds only the two files under
`scripts/version/`, prints "Rules from main at 10a60bf…", passes with `version:none` and fails with
`bug`, naming the four labels; the temporary commit (d1f1513) makes the mode `labels` of its own
copy succeed without any label. On GitHub (pull request 36, 2026-11-10): with `version:none` the
check is green (runs 14 to 17); with the temporary commit 76a560c and the label removed, run 18 is
red, prints "Rules from main at 10a60bf94fd8a6e12a334059c41effb5caecb1ca" and fails at
`scripts/version/NextVersion.cmake:124` with the message that names the four labels; line 124 is
main's copy (the copy in the pull request has seven lines more before it and succeeds earlier).

## O1 - design - major

**A pull request that moves the script or changes how it is called cannot pass its own check.**

The pull request's own `version.yml` runs main's copy of the script (`version.yml:35`,
`version.yml:45`). That is the point for the rules, but it also applies to the interface: a pull
request that renames or moves `scripts/version/NextVersion.cmake`, renames the mode `labels`, or
adds a required parameter, and adapts `version.yml` to it in the same pull request, calls main's
script the new way and fails with "does not exist" or an unknown mode, whatever its label. A re-run
cannot help, and the ruleset for main has an empty bypass list, so nobody can merge it. Decision 2
keeps a change of the versioning possible on purpose; for this kind of change the only ways are two
pull requests (the new interface first beside the old one, then the switch) or the owner changing
the ruleset for a moment. Neither the story nor README says so, and the first person to hit it sees
only a red required check.

## O2 - design - minor

**An accidental change of `version.yml` itself still passes unnoticed.**

Decision 2 treats a change of `version.yml` as deliberate. Changes of workflows are not always: the
follow-up work agents/ci-node24 (pull request 33, 2026-11-09) rewrote every `uses:` line of
`ci.yml` and removed a whole step; the same kind of edit on `version.yml` (for example a new major
of `actions/checkout` with its `with:` block replaced) could drop `ref: main` and bring back the
pull request's own copy without anyone noticing, since the check stays green. A small `ctest` case
(`S032_…`, like the lint test of S000) that reads `version.yml` and asserts that the label job
checks out `main`, runs `NextVersion.cmake` and that the file has no `pull_request_target` would
catch such an accident in the same pull request, because an accidental edit rarely changes the
test as well; a deliberate change would adapt the test with it.

## O3 - failure - minor

**A green check is not refreshed when main's rules change before the merge.**

The check runs on the pull request's events (`version.yml:9-11`), not when main moves. If a pull
request into main changes the rules in `NextVersion.cmake` (decision 3: they apply from the next
pull request on), every pull request that was checked before keeps its result from the old rules.
The ruleset (export of 2026-11-10: pull request, 0 approvals, merge only, check "Version label",
no force push, no deletion) does not require a branch to be up to date before the merge, so such a
pull request can be merged without being checked under the new rules. The tag job, which runs main's
script after the merge, still stops at a merge that the new rules reject; the damage is a red tag
job instead of a red check, as with O3 of S030. Not stated in the story's decision 3.

## O4 - operational - minor

**The temporary commit of the verification turns the S030 cases in `ci.yml` red.**

The verification asks for a commit that makes the mode `labels` always succeed. The `ctest` cases
in `tests/version/s030_version_cases.cmake:127-133` expect exit code 1 without a label and with two,
so the build jobs of `ci.yml` (Linux twice, Windows, macOS) turn red on that push and use their
minutes until they are cancelled. `ci.yml` cancels a running run of the same ref when the next push
comes (`concurrency`, `cancel-in-progress: true`), so the revert should follow as soon as the check
"Version label" has shown its result; the instructions for the verification should say so, and that
the red build jobs of that push are expected.

## Explicitly not objecting to

- **`pull_request` instead of `pull_request_target`, read-only token, no stored credentials.** The
  job checks out main only, runs no code of the pull request and keeps `contents: read`
  (`version.yml:23-24`, `version.yml:38`); the labels still come from the event.
- **Main's tip instead of the base commit.** Decision 1 explains why: GitHub does not refresh
  `pull_request.base.sha` on every update; `ref: main` matches `branches: [main]` of the trigger.
- **The non-cone sparse checkout.** If `NextVersion.cmake` one day needs a file outside
  `scripts/version/`, the check fails with "does not exist" instead of passing; it fails closed.
- **Protection only against accidents.** Decision 2 keeps a deliberate change of `version.yml`
  possible on purpose, for a later change of the versioning.
- **ADR 0010 unchanged.** It describes a label check, not where its script comes from; the story
  and README carry the detail.
- **No new `ctest` case for the job itself.** The rules are covered by the S030 cases; the job's
  wiring can only be shown on GitHub, which the story's own pull request does.
