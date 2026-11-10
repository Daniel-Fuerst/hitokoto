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
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: design
    severity: minor
    title: "An accidental change of version.yml itself still passes unnoticed"
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: minor
    title: "A green check is not refreshed when main's rules change before the merge"
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: operational
    severity: minor
    title: "The temporary commit of the verification turns the S030 cases in ci.yml red"
    disposition: pending
    disposition_rationale: null
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
copy succeed without any label. The run on GitHub follows with the story's pull request.

## O1 - design - major

**A pull request that moves the script or changes how it is called cannot pass its own check.**

The pull request's own `version.yml` runs main's copy of the script (`version.yml:35`,
`version.yml:45`). That is the point for the rules, but it also applies to the interface: a pull
request that renames or moves `scripts/version/NextVersion.cmake`, renames the mode `labels`, or
adds a required parameter, and adapts `version.yml` to it in the same pull request, calls main's
script the new way and fails with "does not exist" or an unknown mode, whatever its label. A re-run cannot
help, and the ruleset for main has an empty bypass list, so nobody can merge it. Decision 2 keeps a
change of the versioning possible on purpose; for this kind of change the only ways are two pull
requests (the new interface first beside the old one, then the switch) or the owner changing the
ruleset for a moment. Neither the story nor README says so, and the first person to hit it sees
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
