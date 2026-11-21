# S042 — Pull requests that change only documentation skip the build

- **Status:** Draft · **Order:** 45 · **Size:** S · **Depends on:** S030, S032, S035
- **Basis:** Armin, 2026-11-21, with the specs of S040 and S041: a pull request that changes only
  stories, reviews or other documentation runs the whole CI (builds and tests on Linux, Windows and
  macOS, fuzzing, clang-tidy, benchmarks) before it may be merged, and again on main after the
  merge, although nothing that is built changed. Facts checked on main `c8a167f`:
  - `ci.yml` runs on `pull_request` without `types`, so only when a pull request is opened,
    reopened or gets new commits; a label set later starts no run. `version.yml` already listens to
    `labeled` and `unlabeled`.
  - Two jobs of `ci.yml` run as a matrix (`build-test`, `bench`). A job skipped as a whole never
    reports the checks of its matrix entries, so required checks with those names would wait
    forever; a job skipped by a condition reports success, and a skipped check counts as passed
    for a required check (GitHub's documentation on required status checks).
  - A path filter on `push` keeps a workflow from running on main; required checks apply to pull
    requests only, so nothing waits for it there.
  - Two tests read a file under `docs/`: `test_s005_theme` and `test_s024_front_matter` open
    `docs/theming.md` to check that every theme token is documented. A change to that file can
    turn the CI red, so it is not documentation in the sense of this story.
  - The ruleset prepared for main requires ten checks one by one: "Version label" and nine jobs of
    `ci.yml`, among them the four matrix entries of "Build and test".

## Story

As a maintainer, I want a pull request that changes only documentation to be mergeable without
waiting for builds and tests, and without a CI run on main after its merge, while the CI still
proves that it changed only documentation.

## Decisions (Armin, 2026-11-21)

1. **A fifth version label, `version:doc`.** It means: the pull request changes only documentation
   in the sense of decision 2. For versions it counts as `version:none`: the merge gets no tag.
   "Version label" accepts exactly one of the five labels. ADR 0010 gets a dated amendment, the
   README's "Versions" the new label; Armin creates the label on GitHub. The pull request that
   implements this story still carries `version:none`, because the check takes its rules from main
   (S032); `version:doc` can be used from the next pull request on.
2. **What counts as documentation.** Every file under `docs/` except the files that tests read. Files
   outside `docs/` never count, also not `README.md`, `CLAUDE.md` or `LICENSE`: a change there runs
   the whole CI (Armin: a simple rule is worth that).
3. **Tests read documentation only through one list.** `tests/docs-read-by-tests.txt` names the files
   under `docs/` that tests, benchmarks or fuzzers read, one path per line; today
   `docs/theming.md`. Tests reach such a file only through a helper that CMake generates from the
   list (working name `test::docsFile("theming.md")`), so that a name not on the list does not
   compile; the two tests of today use it. A lint fails when a source under `tests/`, `bench/` or
   `fuzz/` builds a path into `docs/` from the source directory in any other way. So a new test that
   reads a document cannot be forgotten: it needs the list, and the list excludes the file from
   decision 2.
4. **The pull request check.** `ci.yml` also runs on `labeled` and `unlabeled`, so that setting or
   removing the label starts a new run (the running one is cancelled, as for a new push). A first,
   light job "Changes" looks at the pull request:
   - Without `version:doc` it does nothing, and every job runs as today, also when only
     documentation changed.
   - With `version:doc` it lists the files the pull request changes against its base (three-dot
     diff, renames as a removed and an added file, so that a file moved into `docs/` counts with its
     old path). If every file counts as documentation (decision 2), all other jobs of `ci.yml` are
     skipped. If not, "Changes" fails and names each file that does not count, and the other jobs
     are skipped as well: the label is wrong, and a full run follows once it is changed.
5. **One result for the ruleset.** A last job "CI result" waits for every job of `ci.yml` and always
   runs. It passes when every job passed, or when "Changes" found a pull request with `version:doc`
   that changes only documentation and every other job was skipped; in every other case it fails (a
   failed or cancelled job, or a wrong `version:doc`). After the merge of this story, the ruleset of
   main requires "Version label" and "CI result" instead of the ten checks one by one; this is as
   strict, because "CI result" fails whenever any job fails. The ruleset prepared before stays valid
   until then. The implementation delivers the changed ruleset and the steps to apply it; GitHub
   accepts a required check only after it has run once, which the merge provides.
6. **No CI run on main after a documentation merge.** The `push` trigger of `ci.yml` gets a path
   filter: all paths, then `!docs/**`, then each file of the list again (later patterns override
   earlier ones). A merge that changes only documentation starts no CI run on main, so "Version"
   (S035) does not run for it either, which is right: it gets no tag. The next merge with a version
   tags as today and passes over the documentation merges as `version:none`. A lint fails when the
   path filter and the list disagree. GitHub runs a filtered workflow anyway when a push holds more
   than 1,000 commits, and does not run it when a diff of more than 3,000 files has its matching
   files beyond the first 3,000; neither happens with merges of pull requests here.

## Acceptance scenarios

**Scenario: a documentation pull request is merged without the build**
- Given a pull request that changes `docs/stories/040-table-selection.md` and adds
  `docs/reviews/040-table-selection-spec.md`, labelled `version:doc`
- When its CI runs
- Then "Changes" and "CI result" pass, every other job of `ci.yml` is skipped, and "Version label"
  passes
- When it is merged
- Then no CI run starts on main, and no version tag is set

**Scenario: a wrong `version:doc` is red**
- Given a pull request labelled `version:doc` that also changes `src/ui/table_view.cpp`, or
  `docs/theming.md`, or `README.md`, or moves a file from `src/` into `docs/`
- When its CI runs
- Then "Changes" fails and names each file that does not count as documentation, the other jobs
  are skipped, and "CI result" fails
- When the label is changed to another version label
- Then a new run starts and runs every job

**Scenario: without the label nothing changes**
- Given a pull request that changes only documentation and is labelled `version:none`
- When its CI runs
- Then every job runs as before this story, and "CI result" passes once they all pass

**Scenario: setting or removing the label starts a new run**
- Given an open pull request whose CI ran every job
- When `version:doc` is set (and it changes only documentation)
- Then a new run starts and skips the jobs as in the first scenario
- When the label is removed again
- Then a new run starts and runs every job

**Scenario: "CI result" follows every job**
- Given a pull request without `version:doc`
- When any job of `ci.yml` fails or is cancelled
- Then "CI result" fails

**Scenario: a test that reads a new document needs the list**
- Given a new test that opens `docs/performance.md` through the source directory
- Then the lint fails and names the file and the line
- When the test uses the generated helper with a name that is not on the list
- Then it does not compile
- When `docs/performance.md` is added to the list
- Then the test builds and runs, a pull request labelled `version:doc` that changes that file is
  red, and the push filter of `ci.yml` must name it too, or its lint fails

**Scenario: the version rules know `version:doc`**
- Given the labels of a pull request
- Then "Version label" passes with exactly one of `version:major`, `version:minor`,
  `version:patch`, `version:none` and `version:doc`, and fails with none of them or with two
- And the next version after a merge labelled `version:doc` is "none", as for `version:none`

## Theming

No visible element; nothing to theme.

## Not in this story

Counting `README.md`, `CLAUDE.md` or `LICENSE` as documentation; skipping the CI for other kinds of
change (workflows, scripts); a merge queue (`merge_group`); changes to "Packaging", which follows
"Version" and so does not run after a documentation merge anyway.

## Tests

- `tests/workflows`: `S042_*` lints over `ci.yml`, test first: the pull request trigger includes
  `labeled` and `unlabeled`; every job but "Changes" and "CI result" carries the condition on the
  result of "Changes"; "CI result" needs every other job and always runs; the push filter of main
  equals `!docs/**` plus the list; the S035 lints still hold ("Version" after a green CI on main).
  Each lint is shown red on a broken copy of the workflow, as the S032 lints are.
- `tests/version`: `S030_*` cases for `version:doc` (accepted alone, refused beside another version
  label, next version "none").
- Lint `S042_TestsReadDocsOnlyThroughTheList` over `tests/`, `bench/` and `fuzz/`, red on a sample
  that builds a path into `docs/` directly; a compile check that the generated helper refuses a name
  not on the list.
- `actionlint` and `shellcheck` over the changed workflows without new findings.
- After the merge, by hand: the first pull request labelled `version:doc` is merged without the
  build, and no CI run starts on main; the result is recorded in this story.
