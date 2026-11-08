# S030 — A version for every merge into main

- **Status:** Open · **Order:** 31 · **Size:** M · **Depends on:** S000 ·
  **Decision:** ADR 0010 (proposed)
- **Basis:** `unrefined.md`, "Versioning, and packages and installers for every version" (Armin,
  2026-10-31), refined by Armin on 2026-11-08: versions are set automatically; every final pull
  request into main is a new version; what is open is how major, minor and patch are told apart.
  Proposal (Claude, 2026-11-08): one label per pull request. This story refines the version part
  only. Packages and installers for every version stay in `unrefined.md` and S012; they need the
  version this story gives them (Armin waits with an automatic Debian package for trixie until
  versions exist).

## Story

As the maintainers (Daniel and Armin), we want every pull request merged into main to give hitokoto
a new version number automatically, so that every build says which state it is, and packages built
later can be told apart and updated.

## Decisions (proposed, to be confirmed by Armin and Daniel)

1. **A version per pull request into main.** Merging a pull request whose base is `main` makes a
   new version. Pull requests into other branches (such as #30 into `feature/smooth_scrolling`) do
   not; their changes get a version when that branch's pull request into main is merged.
2. **The pull request says the level, with exactly one label:** `version:major`, `version:minor`
   or `version:patch`. A check on every pull request into main is red unless exactly one of them is
   set. What each level means is part of ADR 0010.
3. **The tag is the only source of the version.** After the merge, a workflow puts an annotated tag
   `vMAJOR.MINOR.PATCH` on the merge commit. No file in the repository is edited for a version (no
   bump commit); `project()` in `CMakeLists.txt` no longer carries a hand-set `VERSION`.
4. **Builds between tags say how far they are from the last version.** CMake derives the version
   at configure time from `git describe`: on a tagged commit `0.4.0`, three commits later
   `0.4.0+3.gabc1234`, with uncommitted changes `.dirty` appended (SemVer build metadata).
   Commits pushed to main directly (such as 083b03e and 6e0eb29, stories and notes) make no new
   version; builds of them carry the distance.
5. **Starting point.** Before the workflow first runs, `v0.3.0` is set by hand on main 705c578, the
   state Daniel named 0.3.0 in debe80e. Only tags of the exact form `vMAJOR.MINOR.PATCH` count; the
   existing tag `v0.3.0-tulip` stays and is ignored.
6. **Builds without git** (a source archive) take the version from `-DHITOKOTO_VERSION=…`;
   without it they get `0.0.0+unknown` and a CMake warning.

## Acceptance scenarios

**Scenario: a pull request into main needs exactly one version label**
- Given a pull request into main without a version label, or with two of them
- Then the check "Version label" is red and names the three labels
- And with exactly one of them it is green
- And for a pull request into another branch the check does not apply

**Scenario: merging makes the next version**
- Given the highest version tag reachable from main is `v0.3.1`
- When a pull request labelled `version:patch` is merged, the merge commit gets `v0.3.2`
- And with `version:minor` it gets `v0.4.0`, with `version:major` it gets `v1.0.0`

**Scenario: only exact version tags reachable from the merge count**
- Given the tags `v0.3.0`, `v0.3.0-tulip`, `v0.10.0-rc1`, `release-2` and a `v9.0.0` on a commit
  that is not an ancestor of the merge commit
- Then the base for the next version is `v0.3.0` (numeric comparison, so `v0.10.0` would beat
  `v0.9.0`)

**Scenario: a missing label after the merge is visible and repairable**
- Given a pull request merged into main without exactly one version label (the check was not
  required, or the label was removed)
- Then the tag job on main is red, says which pull request lacks the label, and sets no tag
- And after the label is set, re-running the job sets the tag

**Scenario: merges in quick succession each get their version**
- Given three pull requests merged into main within a minute
- Then each merge commit gets its own version, in merge order, also when GitHub cancels a pending
  run of the tag job (a run tags every merge commit of a pull request into main since the last
  version tag, oldest first)
- And a run of the CI workflow on main never cancels the tag job (`ci.yml` cancels in-progress runs
  per ref)

**Scenario: tagging is idempotent**
- Given a merge commit that already has a version tag
- When the tag job runs again for it
- Then nothing changes and the job is green

**Scenario: the build reports the version**
- Given a working copy configured on the commit tagged `v0.4.0`
- Then `QApplication::applicationVersion()` is `0.4.0`
- And three commits later it is `0.4.0+3.g` followed by the abbreviated hash, with uncommitted
  changes `.dirty` at the end
- And macOS `CFBundleShortVersionString` and `CFBundleVersion` are `0.4.0` (numbers only)
- And `CPACK_PACKAGE_VERSION` carries the full version

**Scenario: builds without git**
- Given a source tree without `.git`
- When it is configured with `-DHITOKOTO_VERSION=0.4.0`, the version is `0.4.0`
- And without it the version is `0.0.0+unknown` and CMake prints a warning

**Scenario: CI builds see the tags**
- Given a CI job that builds an artefact that carries the version
- Then its checkout fetches the history and tags, and its build does not report `0.0.0+unknown`

**Scenario: documented**
- Then `CLAUDE.md` and `README.md` say that the version comes from the tag, which label to choose
  (the meaning from ADR 0010), and that `VERSION` in `project()` and tags by hand are no longer used

## Requirements

- **No new language, no new dependency.** The logic lives in CMake scripts:
  `cmake/HitokotoVersion.cmake` (configure time: `git describe` → full version, numeric core) and
  `cmake/NextVersion.cmake` (`cmake -P`: list of tags, label → next tag, or an error). Both are
  tested with ctest through `cmake -P` on fixed inputs, so the tests need no git history and run on
  all three CI platforms. Test names carry the story ID (`S030_…`).
- **Workflow:** a new file `.github/workflows/version.yml`, separate from `ci.yml`. Job "Version
  label" on `pull_request` (opened, reopened, edited, labeled, unlabeled, synchronize) for base
  `main`, read-only token, never `pull_request_target`. Job "Tag" on push to main with
  `contents: write`: finds the pull request of each untagged merge commit through the GitHub API,
  reads its labels, computes the next version with `NextVersion.cmake` and pushes the tag with
  `GITHUB_TOKEN`. Its concurrency group never cancels a running run.
- **Tags pushed with `GITHUB_TOKEN` start no other workflow** (GitHub's protection against loops).
  Builds that should run for a new version (packages, S012) must run in the same workflow after the
  tag job or be started with `workflow_dispatch`. Recorded here for the package story.
- **Checkouts:** `actions/checkout` fetches one commit without tags by default; jobs whose artefacts
  carry the version use `fetch-depth: 0`. The test jobs in `ci.yml` keep the shallow checkout and
  accept `0.0.0+unknown`.
- **Repository settings** (Daniel as owner): the check "Version label" as a required status check
  for main; only merge commits allowed (the project merges with a merge commit after green CI).
- Changes under `.github/` need Armin's approval; Armin copies the workflow by hand.

## Not in this story

- Packages, installers and their upload (S012, `unrefined.md`).
- GitHub Releases with release notes; a `CHANGELOG.md`.
- A place in the UI where the user sees the version (Help › About, `--version`); see open questions.
- Signing.

## Open questions (for Daniel and Armin)

1. Label names, and whether pull requests that change only stories, notes or CI also make a version
   (Armin: every pull request into main is a version).
2. Required status check and "merge commits only" as repository settings (Daniel).
3. The starting tag `v0.3.0` on 705c578, and what `v0.3.0-tulip` means: are release names such as
   "tulip" meant to continue?
4. Where should the user see the version? A story of its own.
5. A GitHub Release with generated notes for every version now, or later together with packages?
