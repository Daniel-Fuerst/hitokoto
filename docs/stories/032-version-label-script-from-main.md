# S032 — The check "Version label" takes its script from main

- **Status:** In progress (since 2026-11-10); confirmed by Armin, 2026-11-09 · **Order:** 33
  (after S031) · **Size:** S · **Depends on:** S030 · **Decision:** ADR 0010 (unchanged)
- **Basis:** code review of S030, O3 (`docs/reviews/030-automatic-versions-code.md`), and the entry
  "The check 'Version label' with the script from main" in `unrefined.md`, which this story
  replaces. Follow-up work to S030, written as a story on Armin's request (2026-11-09); implemented
  in a session of its own after S031.

## Story

As the maintainers, we want the required check "Version label" to apply the rules from main, so
that a pull request that changes `scripts/version/NextVersion.cmake` by mistake cannot pass its own
check without a version label.

## Decisions (Armin, 2026-11-09)

1. **The check runs `NextVersion.cmake` from main's tip**, not from the pull request and not from
   the pull request's base commit (GitHub does not refresh `pull_request.base.sha` on every update
   of a pull request; S029 ran into this).
2. **A guard against accidents, not against intent.** GitHub runs `version.yml` for a pull request
   from the pull request itself, so a pull request can still change or remove the check there.
   That stays possible on purpose, for example to move to another way of versioning; removing the
   check also takes it out of the required checks of the ruleset. Closing that too would need
   `pull_request_target`, which S030 rules out. A change that moves `NextVersion.cmake` or changes
   how it is called takes two pull requests: first the new interface beside the old one, then,
   after its merge, the switch in `version.yml`; in one pull request its check would call main's
   script the new way and never pass (code review O1, Armin, 2026-11-10).
3. **A pull request that changes the rules in the script** is checked with main's rules; its own
   rules apply from the next pull request on. A pull request keeps the result of its last run: when
   a merge changes the rules, they apply to open pull requests from their next event on (a push, a
   label, or a re-run of "Version label"); until then the tag job, which runs main's script, stops
   after the merge at a pull request that the new rules reject (code review O3, Armin, 2026-11-10).

## Acceptance scenarios

**Scenario: the check uses main's script**
- Given a pull request into main without a version label that changes `NextVersion.cmake` so that
  the mode `labels` always succeeds
- Then the check "Version label" is red and names the four labels

**Scenario: unchanged behaviour otherwise**
- Given a pull request into main with exactly one version label
- Then the check is green; without one, or with two, it is red and names the four labels (as in
  S030)

**Scenario: documented**
- Then the requirement "Workflow" of S030 and README, "Versions", say that the check takes its
  script from main

## Requirements

- `.github/workflows/version.yml`, the documentation and a lint of the workflow file change. The
  job "Version label" checks out `main` (only `scripts/version/`, read-only token) instead of the
  pull request's merge ref; it still reads the labels from the event, never runs code of the pull
  request, and stays on `pull_request`, never `pull_request_target`.
- The `ctest` case `S032_TheLabelCheckUsesMainsScript` runs `tests/version/s032_workflow_lint.cmake`
  on `version.yml`: no `pull_request_target`, and the job "Version label" checks out `main` once,
  without stored credentials, and runs `NextVersion.cmake` in the mode `labels`. The cases
  `S032_LintFinds*` show that it finds each deviation. It guards against an accidental edit of the
  workflow; a deliberate change adapts it (code review O2, Armin, 2026-11-10).
- Verification: actionlint without findings, and the story's own pull request, once with a
  temporary commit that makes the script always succeed and no label (red), then without it and
  with `version:none` (green). The temporary commit is reverted before the merge.
- Changes under `.github/` need Armin's approval; the label of the pull request is `version:none`.

## Not in this story

- Protection against deliberate changes of `version.yml` (decision 2).
- The tag job, which already runs the script from main.
