# S032 — The check "Version label" takes its script from main

- **Status:** Confirmed (Armin, 2026-11-09) · **Order:** 33 (after S031) · **Size:** S ·
  **Depends on:** S030 · **Decision:** ADR 0010 (unchanged)
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
   `pull_request_target`, which S030 rules out.
3. **A pull request that changes the rules in the script** is checked with main's rules; its own
   rules apply from the next pull request on.

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

- Only `.github/workflows/version.yml` and the documentation change. The job "Version label" checks
  out `main` (only `scripts/version/`, read-only token) instead of the pull request's merge ref; it
  still reads the labels from the event, never runs code of the pull request, and stays on
  `pull_request`, never `pull_request_target`.
- Verification: actionlint without findings, and the story's own pull request, once with a
  temporary commit that makes the script always succeed and no label (red), then without it and
  with `version:none` (green). The temporary commit is reverted before the merge.
- Changes under `.github/` need Armin's approval; the label of the pull request is `version:none`.

## Not in this story

- Protection against deliberate changes of `version.yml` (decision 2).
- The tag job, which already runs the script from main.
