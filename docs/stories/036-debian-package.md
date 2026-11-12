# S036 — Every version gets a Debian package for trixie

- **Status:** Confirmed (Armin, 2026-11-12, after the review
  `docs/reviews/036-debian-package-spec.md` and its dispositions) · **Order:** 37 · **Size:** M · **Depends on:** S030, S034, S035
- **Basis:** Armin wants a Debian package for trixie built by CI for every version (2026-11-08,
  `unrefined.md`, "Packages and installers for every version"); after S034 and S035 (2026-11-12)
  it comes as the asset of a GitHub release per version, and Armin's server fetches it from there.
  The CPack configuration in `CMakeLists.txt` (pull request 15) makes a `.deb` with the binary only
  (no desktop entry, icon or copyright file), and no CI job runs it. The tags so far (`v0.3.0` to
  `v0.5.1`) carry no Debian recipe, so the recipe lives beside the AUR recipe and is applied to the
  tagged source, like S034 does. Decisions of Armin (2026-11-12): the first package is 0.5.1, older
  ones are allowed; the server finds the newest package through GitHub's API, so the file name
  carries the version; the changelog is generated from Git, from 0.3.0 on (no versions before
  that); the maintainer is Daniel, as in the `PKGBUILD`; CPack no longer makes a `.deb`.

## Story

As someone who runs hitokoto on Debian trixie, I want a `.deb` for every new version, built
against Debian's own Qt and installable with `apt`, so that I install and update hitokoto like any
other package.
As the maintainers, we want every version to get its GitHub release with that package without a
manual step, and never a package that does not install and start.

## Decisions

1. **Trigger and place.** Jobs of their own in `packaging.yml`, beside the AUR jobs and independent
   of them: after every successful CI run on a push to main (S035 decision 4), on the commit that
   run passed. The AUR and the releases do not wait for each other; a red AUR job leaves the
   releases alone and the other way round.
2. **The releases are the state; the job only catches up.** The plan takes every version tag of the
   exact form `vMAJOR.MINOR.PATCH` reachable from that commit, from the first version `0.5.1` on
   (a setting in the recipe; lowering it makes the next run release older versions too), and the
   releases of the repository. Every such version without a published release gets one, oldest
   first. A draft left by a failed run is deleted and made again (decision 8). A published release
   is never changed; one without the package is reported in the job summary and left alone. So a
   `version:none` merge releases nothing, and a missed or failed run is caught up by the next one.
   Each version is built, tested and released on its own (one matrix entry per version, no
   fail-fast): a failing version leaves the others alone. A version that cannot be built or tested
   with Debian's Qt for good is listed in the recipe as skipped, with its reason; the plan leaves
   it out and names it in the job summary (review O2).
3. **One recipe, in the repository.** `packaging/debian` holds the recipe: the fields of the control
   file, the copyright file in the machine-readable format (`GPL-3.0-or-later`, referring to
   `/usr/share/common-licenses/GPL-3` as Debian Policy 12.5 asks, review O9), the first version
   and the skipped versions. The desktop entry stays where the AUR recipe needs it,
   `packaging/archlinux/hitokoto.desktop`, and the Debian recipe takes it from there: one file for
   both, and `makepkg -si` in `packaging/archlinux` keeps working (review O1). The recipe comes
   from the commit of decision 1 and is applied to the source of the tag; a fix to the recipe
   reaches the next released version. A recipe change alone (same version) releases nothing.
4. **Built against Debian's Qt, tested first.** In the container `debian:trixie`, with packages
   from the Debian archive only (Qt 6.8.2), the source of the tag is configured in Release with the
   version `X.Y.Z`, built, and its tests run (`ctest`, platform `offscreen`, benchmarks off):
   Qt 6.8.2 is tested nowhere else. The container gets what the tests need from trixie as well:
   googletest (`libgtest-dev`) and a font (`fonts-dejavu-core`), because `debian:trixie` has none
   and the UI tests depend on font metrics (review O5). A failing test stops that version; nothing
   is released for it. A timing test that fails by chance is handled like in the CI: "Re-run failed
   jobs".
5. **The package.** `hitokoto_X.Y.Z-1_amd64.deb`, package version `X.Y.Z-1`, architecture
   `amd64`, section `editors`, maintainer `Daniel Fürst <daniel@fuerst.priv.at>` (as in the
   `PKGBUILD`, review O6). It installs the binary under `/usr/bin`, the desktop entry, the
   scalable icon under `/usr/share/icons/hicolor/scalable/apps`, `copyright` and
   `changelog.Debian.gz` under `/usr/share/doc/hitokoto`. `Depends` comes from `dpkg-shlibdeps`, so
   it names the trixie libraries the binary links, plus `qt6-qpa-plugins` and `qt6-svg-plugins`:
   no library links the platform plugin or the SVG icon engine, and Debian has them only as
   Recommends (assumption, 2026-11-12, not asked). Files owned by root, timestamps from the tagged
   commit (`SOURCE_DATE_EPOCH`), `lintian` without errors (warnings in the job summary).
6. **Changelog from Git.** A script generates the changelog in Debian's format from 0.3.0 up to the
   version: one entry per version tag, listing the merges into main on the first-parent line since
   the previous version tag (`#N` and the pull request's title from the merge commit), commits
   without a merge with their subject; 0.3.0 says that it is the first version with a version tag.
   The entry's date is the date of the tagged commit, the maintainer the one in the control file.
   The same entry, without the Debian frame, is the release notes.
7. **Installed and started before it is released.** A second job in a fresh `debian:trixie`
   container installs the package with `apt-get install ./hitokoto_X.Y.Z-1_amd64.deb` (so a
   missing dependency fails here), checks that `hitokoto --version` prints `hitokoto X.Y.Z`
   (platform `offscreen`), validates the desktop entry (`desktop-file-validate`), and checks that
   after `apt-get purge` no file `dpkg -L` listed for the package is left (review O10).
8. **The release job.** Only the release job has `contents: write`; it builds and runs nothing and
   uses no secret beyond `GITHUB_TOKEN`. Only this job sees drafts (GitHub lists them only with
   push access, review O4): per version it first deletes a draft of the tag left by a failed run,
   then creates a draft release for the existing tag (never a new tag), uploads the package and
   `SHA256SUMS`, sets the notes of decision 6, and publishes it; only the highest version of the
   repository becomes "latest", a caught-up older one never. It runs in a concurrency group
   `releases` without cancelling and checks before each release that none exists yet. Draft first,
   then publish, also works if the repository turns on immutable releases.
9. **The decision logic is a script with tests.** Which versions to release (decision 2), the
   changelog and the notes (decision 6) and the control file (decision 5) come from a CMake script
   under `scripts/packaging/`, run with `cmake -P`; `ctest` cases `S036_*` cover it on all three
   CI platforms, and lints over `packaging.yml` keep the permissions and triggers of decisions 1
   and 8. The workflow gathers the inputs (tags, merges, releases) and acts on the result.
10. **Dry run.** A pull request that changes `packaging/debian`,
    `packaging/archlinux/hitokoto.desktop`, `scripts/packaging` or `packaging.yml`, and a manual run
    without "publish", build, test and install the package for the highest version tag, print which
    versions would be released with their notes, and release nothing, without `contents: write`. A
    manual run with "publish" on main releases what is missing, like a run after the CI.
11. **Fetching.** The server asks
    `https://api.github.com/repos/Daniel-Fuerst/hitokoto/releases/latest` (no token needed), takes
    the asset `hitokoto_X.Y.Z-1_amd64.deb` and checks it against `SHA256SUMS`.
    `packaging/debian/README.md` documents this; the server itself is out of scope.
12. **CPack makes no `.deb`.** `CMakeLists.txt` drops `DEB` from `CPACK_GENERATOR` and its
    `CPACK_DEBIAN_*` settings; `TGZ` stays for S012. Otherwise a local `cpack` build, versioned
    `0.5.1+3.gabc1234`, would sort above the released `0.5.1-1` and install a package without
    desktop entry, icon or copyright file (review O7).
13. **Out of scope.** A Debian source package, an apt repository, other architectures or releases
    than trixie, signing; a trixie build in the CI of every pull request (`unrefined.md`, review
    O3); the Windows installer and S012's other artefacts.

## Acceptance scenarios

**Scenario: a new version gets its release**
- Given the releases go up to 0.5.1
- When a pull request labelled `version:patch` is merged and its green CI tags `v0.5.2`
- Then "Packaging" builds and tests 0.5.2 against Debian's Qt, installs and starts the package in a
  fresh trixie container, and publishes the release `v0.5.2` with `hitokoto_0.5.2-1_amd64.deb` and
  `SHA256SUMS`, marked as latest
- And the notes list the merges since `v0.5.1`, and the package's `changelog.Debian.gz` has the
  entries from 0.3.0 to 0.5.2

**Scenario: the first run catches up 0.5.1**
- Given no release exists and the highest version tag is `v0.5.1`
- When "Packaging" runs after a green CI on main, also after a `version:none` merge
- Then it publishes the release `v0.5.1` with its package, and no release for a version before 0.5.1

**Scenario: a merge without a new version releases nothing**
- Given every version from 0.5.1 on has its release
- When a pull request labelled `version:none` is merged
- Then the Debian part of "Packaging" is green, says that every version has its release, and
  creates nothing

**Scenario: a missed version is caught up, never made latest**
- Given `v0.5.2` has no release because its run failed, and `v0.5.3` has one
- When the next run of "Packaging" starts
- Then it publishes the release `v0.5.2`, and `v0.5.3` stays latest

**Scenario: a package that does not work is not released**
- Given a test fails against Debian's Qt, the package misses a dependency, `hitokoto --version`
  prints another version, the desktop entry does not validate, or `lintian` reports an error
- Then the run is red and no release is created or changed

**Scenario: one broken version does not block the others**
- Given `v0.5.2` fails its tests against Debian's Qt, and `v0.5.3` has no release either
- When "Packaging" runs
- Then `v0.5.3` is released and the run is red for `v0.5.2`
- And once `0.5.2` is listed as skipped in the recipe with its reason, the next run leaves it out,
  names it in the job summary and is green

**Scenario: a failed release is made again**
- Given a draft release `v0.5.2` left by a run that failed after the upload
- When the next run starts
- Then it deletes the draft and publishes `v0.5.2` anew; a published release is never changed

**Scenario: a dry run releases nothing**
- Given a pull request that changes the Debian recipe, or a manual run with "publish" off
- Then the run builds, tests, installs and starts the package for the highest version tag, prints
  the versions it would release with their notes, and creates no release

**Scenario: the decision logic is tested**
- Then `ctest` runs the cases `S036_*` for the versions to release (none, one, several, drafts,
  published releases, skipped versions, tags not of the exact form, versions before the first
  one), for the changelog and notes (merges, commits without a merge, the first version, the
  dates) and for the control file, on Linux, Windows and macOS
- And the lints fail if a job other than the release job gets `contents: write`, if the release job
  can run on a pull request, or if `CPACK_GENERATOR` names `DEB`

## Notes

- Label of this story's pull request: `version:none` (nothing changes in the program, and the
  CPack change touches no published artefact); the run after its merge releases 0.5.1, so the
  release path is exercised right away and is to be watched.
- `packaging/debian/README.md` explains the recipe, a local build in a trixie container and the
  fetching of decision 11; `README.md` points to it.
- Once a released version installs its desktop entry and icon through CMake (S012 or a later
  story), both recipes can drop their own copies.
- Immutable releases (review O8): with the setting turned on (repository settings, Daniel), assets
  cannot be replaced after publishing and every release gets an attestation that the server can
  check with `gh release verify`; decision 8 already works with it. Suggested to Daniel, not a
  condition of this story.
