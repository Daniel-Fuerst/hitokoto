---
mode: code
spec: docs/stories/036-debian-package.md
commits: c5dff8f..5593b11 (agents/s036-debian-package, cloud copy; S036 implementation)
reviewer: Advocatus Diaboli (Claude, in the session that implemented the story; not an independent
  subagent)
date: 2026-11-13
objections:
  - id: O1
    category: failure
    severity: major
    title: A failing API call reads as "no release" and can create a second release of a tag
    disposition: fixed
    disposition_rationale: >-
      The release step lists the releases of the tag once; a failing API call stops the job
      (8b4e3e9).
  - id: O2
    category: design
    severity: minor
    title: The draft is published by its tag, which a draft does not own
    disposition: fixed
    disposition_rationale: >-
      The draft is published by its id with make_latest, after checking for exactly one draft
      (8b4e3e9).
  - id: O3
    category: failure
    severity: minor
    title: The dry run shows the notes of the highest version only
    disposition: fixed
    disposition_rationale: >-
      The plan writes the notes of every version to release into its summary (8b4e3e9).
  - id: O4
    category: failure
    severity: major
    title: The build in debian:trixie has never run
    disposition: documented
    disposition_rationale: >-
      The dry run on the pull request is the first real build; its fixes come test-first on the
      branch before In review. Whether the two Qt plugin packages are right and complete shows in
      practice (Armin, 2026-11-13).
  - id: O5
    category: operational
    severity: minor
    title: The image debian:trixie floats and comes from Docker Hub
    disposition: documented
    disposition_rationale: >-
      The floating tag is wanted; two pulls per version, rarely.
  - id: O6
    category: cost
    severity: minor
    title: Every version builds and runs the whole test suite once more
    disposition: documented
    disposition_rationale: >-
      Public repository, free runner minutes; catch-ups are rare.
  - id: O7
    category: design
    severity: minor
    title: The label version:none while CMakeLists.txt changes
    disposition: documented
    disposition_rationale: >-
      No published artefact comes from CPack; label version:none (Armin, 2026-11-13).
  - id: O8
    category: failure
    severity: minor
    title: The changelog cases pass without git
    disposition: documented
    disposition_rationale: >-
      Same rule as the S030 case GitRepository; every CI runner has git.
---

# S036 — Every version gets a Debian package for trixie: objections (code)

Implementation on `agents/s036-debian-package` in the cloud copy, c5dff8f..5593b11 (S031 done,
S036 confirmed, red and green commits, `packaging.yml`, docs), read against the story and its spec
review. Checked: ctest S030-S036 134/134 (core-only build, CMake 3.28), actionlint 1.7.12 with
shellcheck 0.11.0, mutations of the lints, both recipe scripts with a stand-in source on Ubuntu
24.04, the release step of `packaging.yml` with a stand-in `gh` (fresh, draft left, published,
API failure, missing package). Dispositions by Armin on 2026-11-13, as proposed.

## O1 - failure - major

**A failing API call reads as "no release" and can create a second release of a tag.** The release
step (5593b11, `packaging.yml`, job `debian-release`) asked `gh api repos/…/releases/tags/$tag` and
took any failure, not only 404, as "no published release". A rate limit or a network error there
on a version that has a release makes the job create a draft for the same tag and publish it: two
releases of one tag, and the server's `releases/latest` may point at either. Decision 2 says a
published release is never changed and never made again.

**Disposition: fixed** (8b4e3e9). The step lists the releases of the tag once (published and draft,
with `--paginate`); a failing call stops the job (`set -e`). Checked with a stand-in `gh` that
fails: the job ends red and creates nothing.

## O2 - design - minor

**The draft is published by its tag, which a draft does not own.** `gh release edit "$tag"
--draft=false` relies on gh finding a draft by its tag name; the tag endpoint of the API never
returns drafts. If two drafts of the tag existed (a race with a run that has not deleted its
draft), which one gh picks is not defined.

**Disposition: fixed** (8b4e3e9). After `gh release create`, the step looks up the drafts of the
tag, requires exactly one, and publishes it by its id with `make_latest` (`gh api -X PATCH`).

## O3 - failure - minor

**The dry run shows the notes of the highest version only.** Decision 10 asks a dry run to print
which versions would be released with their notes. The plan's summary listed the versions, but only
the matrix entry of the highest version wrote notes.

**Disposition: fixed** (8b4e3e9). The plan step writes the notes of every version to release into
its summary (`MODE=changelog` per version), in a dry run and in a real run.

## O4 - failure - major

**The build in debian:trixie has never run.** The cloud and the Cowork VM reach no Debian mirror,
and the cloud has no Docker daemon. Untested are: whether `build-depends` is complete (OpenGL and
Vulkan headers that `qt6-base-dev` pulls, googletest's CMake package from `libgtest-dev`), whether
the tests of v0.5.1 pass with Qt 6.8.2 and DejaVu in a container, and which `lintian` tags the real
binary gets (an error stops the release). The stand-in run only shows that the scripts are right
around the build.

**Disposition: documented.** The dry run on the pull request is the first real run, as for S034; its
fixes come test-first on the branch before the story goes to In review. Whether `qt6-qpa-plugins`
and `qt6-svg-plugins` are the right and complete extra dependencies shows in practice (Armin,
2026-11-13).

## O5 - operational - minor

**The image debian:trixie floats and comes from Docker Hub.** The tag follows Debian's point
releases, so a package built later for the same version can link newer libraries (fine for
trixie users), and every matrix entry pulls the image twice from Docker Hub, whose anonymous pull
limit applies to the runner's address. A pinned digest would freeze the archive state but then
needs updating by hand.

**Disposition: documented.** The floating tag is wanted (the package tracks trixie); the pulls are
two per version and run rarely.

## O6 - cost - minor

**Every version builds and runs the whole test suite once more.** The CI on main has already run
the tests of the tag on Qt 6.8.3 and 6.12; the matrix entry builds again with tests and runs them
with 6.8.2, which is the point of decision 4. On a catch-up of several versions this multiplies.

**Disposition: documented.** The repository is public, so the runner minutes are free; versions are
caught up rarely.

## O7 - design - minor

**The label version:none while CMakeLists.txt changes.** ADR 0010 puts build changes to an artefact
at `version:patch`. The pull request drops `DEB` from CPack in `CMakeLists.txt`.

**Disposition: documented.** No published artefact comes from CPack; the program and the release
packages do not change, so `version:none` fits (the story's notes say so). Armin chose
`version:none` (2026-11-13).

## O8 - failure - minor

**The changelog cases pass without git.** `make_repo()` prints `S036-SKIP` and returns when git is
missing, like the S030 case `GitRepository`. All CI runners have git, so the cases run there.

**Disposition: documented.** Same rule as S030.

## Explicitly not objecting to

- The decision logic in a CMake script with ctest cases on three platforms, as in S034.
- Building from `git archive` of the tag with the recipe of the checked-out commit (decision 3).
- `--root-owner-group`, `SOURCE_DATE_EPOCH` and `gzip -n`: the package does not depend on the
  runner's user or clock.
- The file-list check in `build-package.sh`: an unexpected file stops the build before `dpkg-deb`.
- The second fresh container in the same job instead of a second job (noted in the commit as an
  assumption): only a checked package becomes an artifact.
