# S030 — A defined version step for every merge into main

- **Status:** In review (since 2026-11-09: CI green on Linux, Windows and macOS on ff2aab1, after
  the code review `docs/reviews/030-automatic-versions-code.md`); Confirmed (Armin, with Daniel's
  agreement, 2026-11-09, after the review `docs/reviews/030-automatic-versions-spec.md`) ·
  **Order:** 31 · **Size:** M · **Depends on:** S000 · **Decision:** ADR 0010
- **Basis:** `unrefined.md`, "Versioning, and packages and installers for every version" (Armin,
  2026-10-31), refined by Armin on 2026-11-08: versions are set automatically; every final pull
  request into main is a new version; what is open is how major, minor and patch are told apart.
  Proposal (Claude, 2026-11-08): one label per pull request. On 2026-11-09 Armin and Daniel added
  `version:none` for pull requests that change nothing in the built program. This story refines the
  version part only. Packages and installers for every version stay in `unrefined.md` and S012; they
  need the version this story gives them (Armin waits with an automatic Debian package for trixie
  until versions exist).

## Story

As the maintainers (Daniel and Armin), we want every pull request merged into main to give hitokoto
a new version number automatically, so that every build says which state it is, and packages built
later can be told apart and updated.

## Decisions (confirmed by Armin and Daniel, 2026-11-09)

1. **A version step for every pull request into main.** Every pull request whose base is `main`
   defines its version step with its label (decision 2); merging it makes a new version unless the
   step is `version:none`. Pull requests into other branches (such as #30 into
   `feature/smooth_scrolling`) define none; their changes get a version when that branch's pull
   request into main is merged.
2. **The pull request says the level, with exactly one label:** `version:major`, `version:minor`,
   `version:patch` or `version:none`. A check on every pull request into main is red unless exactly
   one of them is set. `version:none` means no functional change: only documentation or
   infrastructure (definition in ADR 0010); anything that ships in the program is at least
   `version:patch`. A merge with `version:none` gets no tag, and builds after it carry the distance
   to the last version. What each level means is part of ADR 0010.
3. **The tag is the only source of the version.** After the merge, a workflow puts an annotated tag
   `vMAJOR.MINOR.PATCH` on the merge commit. No file in the repository is edited for a version (no
   bump commit); `project()` in `CMakeLists.txt` no longer carries a hand-set `VERSION`.
4. **Builds between tags say how far they are from the last version.** CMake derives the version
   at configure time from `git describe`: on a tagged commit `0.4.0`, three commits later
   `0.4.0+3.gabc1234`, with uncommitted changes `.dirty` appended (`0.4.0+3.gabc1234.dirty`; on
   the tagged commit itself `0.4.0+dirty`; SemVer build metadata).
   Commits pushed to main directly (such as 083b03e and 6e0eb29, stories and notes) make no new
   version; builds of them carry the distance. The version is that of the last configure: an
   incremental build that does not reconfigure (Visual Studio in folder mode, `cmake --build`
   after a pull) keeps the old string. CI builds and package builds always configure afresh.
5. **Starting point.** Before the workflow first runs, `v0.3.0` is set by hand as an annotated tag
   on main 705c578, the state Daniel named 0.3.0 in debe80e. Only tags of the exact form
   `vMAJOR.MINOR.PATCH` count; the existing tag `v0.3.0-tulip` stays and is ignored (release names
   are not part of this story). Apart from this starting tag, nobody sets a version tag by hand:
   such a tag would become the base of every later version. The four labels exist in the
   repository since 2026-11-09 (created by hand), and every pull request merged into main after
   705c578 gets its label by hand before this story is merged: #31 (S029) and #33 (CI) carry
   `version:none`, set by Armin on 2026-11-09; later ones are labelled when they are merged.
6. **Builds without git** (a source archive) take the version from `-DHITOKOTO_VERSION=…`;
   without it they get `0.0.0+unknown` and a CMake warning.

## Acceptance scenarios

**Scenario: a pull request into main needs exactly one version label**
- Given a pull request into main without a version label, or with two of them
- Then the check "Version label" is red and names the four labels
- And with exactly one of them it is green
- And for a pull request into another branch the check does not apply

**Scenario: merging makes the next version**
- Given the highest version tag reachable from main is `v0.3.1`
- When a pull request labelled `version:patch` is merged, the merge commit gets `v0.3.2`
- And with `version:minor` it gets `v0.4.0`, with `version:major` it gets `v1.0.0`
- And with `version:none` it gets no tag, and the next merge with `version:patch` gets `v0.3.2`

**Scenario: the first run starts from the starting tag**
- Given `v0.3.0` on 705c578, and after it on main the merges of #31 and #33, both labelled
  `version:none`
- When the merge of this story's pull request, labelled `version:patch`, runs the tag job for the
  first time
- Then the merges of #31 and #33 get no tag and the merge of this story gets `v0.3.1`
- (`version:patch`, not `version:none`: the pull request changes `src/app/` and the version the
  built program reports; nothing new for users, so not `version:minor`. Armin, 2026-11-09.)

**Scenario: only exact version tags reachable from the merge count**
- Given the tags `v0.3.0`, `v0.3.0-tulip`, `v0.10.0-rc1`, `release-2` and a `v9.0.0` on a commit
  that is not an ancestor of the merge commit
- Then the base for the next version is `v0.3.0` (numeric comparison, so `v0.10.0` would beat
  `v0.9.0`)

**Scenario: only merges of pull requests into main make versions**
- Given main's history since the last version tag holds, on its first-parent line, the merge of a
  pull request into main (like 705c578, PR #26), a commit pushed to main directly (like 083b03e),
  and, reached only through a second parent, the merge of a pull request into another branch (like
  e75458c, PR #30 into `feature/smooth_scrolling`)
- Then only the merge of the pull request into main gets a version
- And the commit pushed directly and the merge into the other branch get none

**Scenario: a missing label after the merge is visible and repairable**
- Given a pull request merged into main without exactly one version label (the check was not
  required, or the label was removed)
- Then the tag job on main is red, says which pull request lacks the label, and sets no tag
- And after the label is set, re-running the job sets the tag

**Scenario: a later merge waits for an earlier merge without a label**
- Given merge A of a pull request without a version label, and after it merge B of a pull request
  with `version:patch`, both untagged
- Then the tag job stops at A: it is red, names A's pull request, and tags neither A nor B
- And after A's label is set, a re-run tags A and then B, in merge order

**Scenario: merges in quick succession each get their version**
- Given three pull requests merged into main within a minute
- Then each merge commit gets its own version, in merge order, also when GitHub cancels a pending
  run of the tag job (a run tags every untagged merge of a pull request into main since the last
  version tag, oldest first)
- And a run of the CI workflow on main never cancels the tag job (`ci.yml` cancels in-progress runs
  per ref)

**Scenario: no version tag, no new version**
- Given no tag of the form `vMAJOR.MINOR.PATCH` is reachable from the merge commit (for example a
  checkout without tags)
- Then computing the next version fails, the tag job is red and sets no tag (it never starts over
  at `v0.0.1`)

**Scenario: tagging is idempotent**
- Given a merge commit that already has a version tag
- When the tag job runs again for it
- Then nothing changes and the job is green

**Scenario: the build reports the version**
- Given a working copy configured on the commit tagged `v0.4.0`
- Then `QApplication::applicationVersion()` is `0.4.0`
- And configured three commits later it is `0.4.0+3.g` followed by the abbreviated hash, with
  uncommitted changes `.dirty` at the end, and on the tagged commit with uncommitted changes
  `0.4.0+dirty`
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
  (no `v*` tag by hand apart from the starting tag `v0.3.0`)
- And they say that the label is part of the review before the merge and must fit the pull
  request's final content, not only what it was opened for
- And they say that the version is that of the last configure
- And they say that changes reach main only through pull requests, and that notes and status changes
  take a pull request with `version:none`

## Requirements

- **No new language, no new dependency.** The logic lives in CMake scripts:
  `scripts/version/HitokotoVersion.cmake` (configure time: `git describe` → full version, numeric
  core) and `scripts/version/NextVersion.cmake` (`cmake -P`: list of tags, label → next tag, no tag
  for `version:none`, or an error). Both are tested with ctest through `cmake -P` on fixed inputs,
  so the tests need no git history and run on all three CI platforms. Test names carry the story ID
  (`S030_…`).
- **Why `scripts/version/` and not `cmake/`** (Armin, 2026-11-09, during the implementation): the
  benchmark job of S029 counts every change under `cmake/` as a toolchain change, which must come in
  a pull request of its own with nothing beside it but `docs/` and `*.md`. These scripts change
  nothing the benchmark measures, and this story's pull request also changes `CMakeLists.txt`,
  `src/app/` and `tests/`; under `cmake/` its benchmark job would fail.
- **Workflow:** a new file `.github/workflows/version.yml`, separate from `ci.yml`. Job "Version
  label" on `pull_request` (opened, reopened, edited, labeled, unlabeled, synchronize) for base
  `main`, read-only token, never `pull_request_target`. Job "Tag" on push to main with
  `contents: write`:
  - Candidates are the commits on main's first-parent line after the last version tag, oldest
    first. A candidate is a version only if a pull request with base `main` has it as its
    `merge_commit_sha` (looked up through the GitHub API for the commit's pull requests); other
    candidates, such as commits pushed directly, are skipped. Commits reached only through a second
    parent are never candidates, whatever pull request merged them elsewhere.
  - A merge commit (two parents) on that line is never skipped silently (code review O1, Armin,
    2026-11-09): if the lookup finds no pull request, the job reads the number from the commit
    message ("Merge pull request #N …" or "… (#N)") and checks that pull request directly; if
    neither confirms it as the merge of a pull request into main, the job stops there and is red,
    like a merge without a label, and a re-run tags it. Only commits with one parent count as pushed
    directly.
  - For each such merge in order it reads the pull request's labels, computes the next version
    with `NextVersion.cmake` and tags the merge commit; a merge labelled `version:none` is passed
    over without a tag. At the first merge without exactly one version label it stops, is red and
    names that pull request; nothing after it is tagged.
  - Tags are pushed with `GITHUB_TOKEN`. Its concurrency group never cancels a running run.
- **Tags pushed with `GITHUB_TOKEN` start no other workflow** (GitHub's protection against loops).
  Builds that should run for a new version (packages, S012) must run in the same workflow after the
  tag job or be started with `workflow_dispatch`. Recorded here for the package story.
- **Checkouts:** `actions/checkout` fetches one commit without tags by default. The tag job and
  jobs whose artefacts carry the version use `fetch-depth: 0`, which brings the tags.
  `NextVersion.cmake` fails when no version tag is reachable instead of assuming `v0.0.0`. The test
  jobs in `ci.yml` keep the shallow checkout and accept `0.0.0+unknown`.
- **macOS bundle versions** carry only the numeric core, so builds between two tags look the same to
  Launch Services and installers. Harmless for builds by hand; S012 distributes tagged builds only.
- **Repository settings** (Daniel as owner): one branch ruleset for the default branch (Settings ›
  Rules › Rulesets), active, with an empty bypass list, so that it applies to everyone including the
  owner. Daniel creates it right away (agreed 2026-11-09) with all rules except the status check,
  and adds the status check after the merge of this story, when the check exists:
  - "Require a pull request before merging": no direct pushes to main; 0 required approvals, so
    that everyone can merge their own pull requests; allowed merge method "merge" only (the project
    merges with a merge commit after green CI).
  - "Require status checks to pass before merging": the check "Version label".
  - "Block force pushes" and "Restrict deletions".
  - The ruleset targets the branch main only: the tags of the tag job and other branches (such as
    S029's `bench-data`) are not affected. The scenario for commits pushed directly stays as a
    safeguard for times the ruleset is off.
- Changes under `.github/` need Armin's approval; Armin copies the workflow by hand.

## Not in this story

- Packages, installers and their upload (S012, `unrefined.md`).
- GitHub Releases with release notes: later, together with packages, so that a release carries
  something to download (Armin, 2026-11-09; `unrefined.md`, "Packages and installers for every
  version"). Until then the versions are listed under the repository's tags. A `CHANGELOG.md`.
- A place where the user sees the version (Help › About, `--version`, the title bar):
  `unrefined.md`, "Showing the version to the user" (Armin, 2026-11-09).
- Signing.

## Questions (all settled)

1. Settled (Armin, 2026-11-09): label names as proposed; every merge into main defines its version
   step, and `version:none` makes no version (Armin and Daniel, spec review O1).
2. Settled (Armin, 2026-11-09): a ruleset for main, see "Repository settings".
3. Settled (Armin, 2026-11-09): the starting tag `v0.3.0` stays on 705c578; the pull requests
   merged into main after it are labelled by hand (decision 5). `v0.3.0-tulip` stays and is
   ignored; whether release names continue is not part of this story.
4. Settled (Armin, 2026-11-09): not in this story; an entry in `unrefined.md`.
5. Settled (Armin, 2026-11-09): GitHub Releases later, together with packages.
