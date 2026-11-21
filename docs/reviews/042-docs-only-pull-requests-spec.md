---
mode: spec
spec: docs/stories/042-docs-only-pull-requests.md
reviewer: Advocatus Diaboli (Claude, in the session that drafted the story; not an independent
  subagent)
date: 2026-11-21
objections:
  - id: O1
    category: failure
    severity: major
    title: Every label change starts a run that cancels the running CI, and a re-run sees old labels
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-21): no label events in ci.yml; "Changes" reads the current labels through the
      API at run time, so a label set when the pull request is opened works at once and one set or
      removed later takes effect with "Re-run all jobs" (decision 4, scenario).
  - id: O2
    category: premise
    severity: minor
    title: Recognising a documentation pull request by its files alone would need no label events
    disposition: resolved
    disposition_rationale: >-
      Armin (2026-11-21): the label stays as a deliberate claim that the CI checks; without it the
      builds run. O1, the cost of the label, is fixed.
  - id: O3
    category: threat
    severity: minor
    title: One required check decided by the pull request's own workflow file
    disposition: documented
    disposition_rationale: >-
      Decision 5 says that the pull request's own ci.yml decides its checks as today, that a change
      to ci.yml stays a matter of review, and that such a pull request can never be version:doc
      (Armin, 2026-11-21).
  - id: O4
    category: failure
    severity: minor
    title: The lint finds paths built from the source directory, not every way to read docs/
    disposition: fixed
    disposition_rationale: >-
      Decision 3: the lint reads the C++ and CMake files under tests/, bench/ and fuzz/ and fails on
      every string holding docs/ outside comments and the helper, relative paths and file(GLOB)
      included; text that only mentions a path is allowed by a marker comment on its line (Armin,
      2026-11-21).
  - id: O5
    category: operational
    severity: minor
    title: A required check can be chosen only after it passed within the last seven days
    disposition: fixed
    disposition_rationale: >-
      Decision 5: the steps to switch the ruleset say that GitHub offers a required check only if it
      passed within the past seven days, so the switch follows a pull request run within a week
      (Armin, 2026-11-21).
---

# Review (spec) of S042 — Pull requests that change only documentation skip the build

## O1 - failure - major

Decision 4 adds `labeled` and `unlabeled` to the pull request trigger of `ci.yml`. These fire for
every label, not only for `version:doc`; `on` cannot filter by the label's name. `ci.yml` cancels a
running pull request run when a new one starts (`concurrency: … cancel-in-progress` for pull
requests), so setting any label, also the version label that every pull request needs, cancels the
full CI that is running and starts it again; a cancelled run leaves "CI result" failed until the new
one ends. A job condition cannot prevent this, because the cancellation happens at the workflow's
concurrency group before any job runs. In addition, "Re-run jobs" repeats a run with its original
event payload, so a run started before `version:doc` was set does not see the label when re-run;
the story reads the label from the event (decision 4 does not say otherwise).

## O2 - premise - minor

The CI could tell a documentation pull request by its files alone: "Changes" computes the list in
any case. Then `ci.yml` needs no label events (O1 disappears), and `version:doc` would only carry
the version meaning, or not exist. The label adds a deliberate claim that the CI checks, which is
Armin's intent ("Ich kann einen Tag setzen, der so einen PR kennzeichnet"); the cost of that claim,
O1, is not weighed in the story.

## O3 - threat - minor

On `pull_request`, GitHub runs the workflow file of the pull request itself. Decision 5 reduces the
required checks of `ci.yml` to one, "CI result", whose logic lives in that file. A pull request that
edits `ci.yml` is not documentation and is red under `version:doc`, but labelled otherwise it can
change how "CI result" decides, and only one check has to be made green. This is not new (today
each of the nine required jobs is defined by the same file), and S032 solved it for "Version label"
by taking the rules from main; the story should say that changes to `ci.yml` stay a matter of
review, or take the decision of "CI result" from main as S032 does.

## O4 - failure - minor

Decision 3's lint fails when a source "builds a path into `docs/` from the source directory".
A test can read documentation in other ways: a relative path (`../docs/…` from the build or test
directory), a CMake `file(GLOB …)` or `configure_file` under `tests/` that copies a document into a
resource, as S031 does with its samples under `tests/data`. The story should list what the lint
covers (C++ sources and CMake files under `tests/`, `bench/`, `fuzz/`; any string naming `docs/`
outside comments and the helper), and accept that test data that only mentions a path, such as the
front matter `spec: docs/stories/022-inline-links.md` in `tst_s024_front_matter`, needs an explicit
exception.

## O5 - operational - minor

Decision 5 says GitHub accepts a required check "after it has run once". GitHub's troubleshooting
page for required status checks says more narrowly that a required check must have completed
successfully in the repository during the past seven days. If the ruleset is switched later than a
week after the last pull request run, "CI result" cannot be chosen until another pull request has
run. The steps delivered with the implementation should say so.

## Explicitly not objecting to

- A fifth label instead of reusing `version:none`: Armin's choice, and `version:none` stays for
  documentation together with tests or infrastructure, which needs the full CI.
- Leaving `README.md`, `CLAUDE.md` and `LICENSE` out: a simple rule, accepted by Armin.
- The push filter on main: required checks do not apply to pushes, and the benchmark job builds its
  base itself (S029), so no later run depends on a CI run of a documentation merge.
- Failing a wrong `version:doc` at once instead of running the build: a wrong label is a mistake to
  fix, and the full run follows the corrected label.
- The list of documents read by tests under `tests/`: it belongs to the tests, and the brief keeps
  `docs/theming.md` where it is (section 8).
