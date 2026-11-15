---
mode: spec
spec: docs/stories/036-debian-package.md
reviewer: Advocatus Diaboli (Claude, in the session that drafted the story; not an independent
  subagent)
date: 2026-11-12
objections:
  - id: O1
    category: design
    severity: major
    title: Moving the desktop entry breaks the local build of the AUR recipe
    disposition: fixed
    disposition_rationale: >-
      The desktop entry stays in packaging/archlinux and the Debian recipe takes it from there
      (decision 3); S034, its tests and the local makepkg stay as they are.
  - id: O2
    category: failure
    severity: major
    title: A version that never builds or tests on Qt 6.8.2 keeps every run red
    disposition: fixed
    disposition_rationale: >-
      Decision 2: one matrix entry per version without fail-fast, and a list of skipped versions
      with their reasons in the recipe; scenario "one broken version does not block the others".
  - id: O3
    category: design
    severity: minor
    title: A break on trixie is found only after the version is tagged
    disposition: documented
    disposition_rationale: >-
      No change to ci.yml now (Armin, 2026-11-12); a trixie build and test in the CI of every pull
      request is noted in unrefined.md, and O2 limits the harm of a break found after the tag.
  - id: O4
    category: failure
    severity: minor
    title: The plan cannot see drafts with contents read
    disposition: fixed
    disposition_rationale: >-
      Decision 8: the release job, the only one with push access, deletes a draft of the tag
      before it creates the release.
  - id: O5
    category: failure
    severity: minor
    title: The tests run in a bare container unlike the CI's runner
    disposition: fixed
    disposition_rationale: >-
      Decision 4 names libgtest-dev and fonts-dejavu-core from trixie; a timing test that fails by
      chance is re-run like in the CI.
  - id: O6
    category: operational
    severity: minor
    title: Maintainer of the package and the changelog is not named
    disposition: fixed
    disposition_rationale: >-
      Decision 5: maintainer Daniel Fürst <daniel@fuerst.priv.at>, as in the PKGBUILD (Armin,
      2026-11-12).
  - id: O7
    category: design
    severity: minor
    title: Two Debian recipes with the same package name
    disposition: fixed
    disposition_rationale: >-
      Decision 12: DEB leaves CPACK_GENERATOR and the CPACK_DEBIAN_* settings go; TGZ stays for
      S012 (Armin, 2026-11-12); a lint checks it.
  - id: O8
    category: threat
    severity: minor
    title: The server installs as root what only a checksum from the same release vouches for
    disposition: documented
    disposition_rationale: >-
      Notes: immutable releases are suggested to Daniel and the check with gh release verify is
      named; not a condition of this story, decision 8 already works with them.
  - id: O9
    category: design
    severity: minor
    title: The copyright file quotes the GPL instead of referring to common-licenses
    disposition: fixed
    disposition_rationale: >-
      Decision 3: the copyright file refers to /usr/share/common-licenses/GPL-3.
  - id: O10
    category: failure
    severity: minor
    title: Removing the package without leftovers is not defined
    disposition: fixed
    disposition_rationale: >-
      Decision 7: after apt-get purge no file that dpkg -L listed for the package is left.
---

# S036 — Every version gets a Debian package for trixie: objections (spec)

Story as first drafted on 2026-11-12 on `agents/s036-debian-package` (from main fe8f0c3, after the
merge of pull request 42), read together with `.github/workflows/packaging.yml`,
`packaging/archlinux/` (`PKGBUILD`, `hitokoto.desktop`, `README.md`), `CMakeLists.txt` (CPack),
`cmake/HitokotoDependencies.cmake`, `scripts/version/HitokotoVersion.cmake`, S012, S034, S035 and
`unrefined.md`. Dispositions by Armin on 2026-11-12 (O3, O6 and O7 decided by him, the others
as proposed); the story carries them.

## O1 - design - major

**Moving the desktop entry breaks the local build of the AUR recipe.** Decision 3 moves
`hitokoto.desktop` to `packaging/linux`. The `PKGBUILD` takes it as a local source next to itself
(`source=(… "hitokoto.desktop")`, second `sha256sums` entry), and S034 decision 3 keeps the
`PKGBUILD` in `packaging/archlinux` so that "a local `makepkg -si` still works", as
`packaging/archlinux/README.md` lines 9-14 tell users to do. After the move `makepkg` there fails
for a missing source. The AUR job (`cp packaging/archlinux/hitokoto.desktop`) and the cases
`S034_TheDesktopEntryPassesOneFile` and `S034_TheDesktopEntryChecksumMatches` (read
`${RECIPE_DIR}/hitokoto.desktop`) have to follow as well, so a Done story changes for a gain in
naming only. A symlink is no way out on Windows checkouts. Options: the Debian recipe reads the
desktop entry from `packaging/archlinux` (one file, an odd path), or a lint keeps two copies equal.

**Disposition: fixed.** The desktop entry stays in `packaging/archlinux`; the Debian recipe takes it
from there (decision 3). S034, its cases and the local `makepkg -si` stay as they are.

## O2 - failure - major

**A version that never builds or tests on Qt 6.8.2 keeps every run red.** The CI builds with
Qt 6.8.3 and 6.12 only; decision 4 builds and tests each version with Debian's 6.8.2, and decision 2
retries every version without a release on every run, oldest first. A version that uses something
6.8.2 lacks, or has a test that fails there for good, is retried after every merge: "Packaging" is
red on every run (and GitHub mails each time), and the story does not say whether later versions
are still built and released in that run. The earlier draft of a releases story raised the same
"lasting red legacy" (hitokoto-Wissen, review of the discarded S034 draft, 2026-11-12). Missing: one
version's failure must not stop the others (matrix without fail-fast), and a way to give up a
version on purpose (for example a list of skipped versions in the recipe, with a reason), so that
the run turns green again.

**Disposition: fixed.** Decision 2: one matrix entry per version without fail-fast; a version that
fails for good is listed as skipped in the recipe with its reason. New scenario "one broken version
does not block the others".

## O3 - design - minor

**A break on trixie is found only after the version is tagged.** The pull request dry run of
decision 10 runs only for changes under `packaging/debian`, `packaging/linux`,
`scripts/packaging` or `packaging.yml`, and builds the highest existing tag, not the pull request.
A pull request that breaks the build or a test on Qt 6.8.2 passes its CI, is merged and tagged, and
only then fails in "Packaging" (feeding O2). A trixie build-and-test job in `ci.yml` on every pull
request would catch it before the merge, at the cost of a change to `ci.yml` (Armin's approval)
and some minutes per run.

**Disposition: documented.** No change to `ci.yml` in this story (Armin, 2026-11-12). A build and
test against Debian's Qt in the CI of every pull request is noted in `unrefined.md` ("Packages and
installers for every version"); decision 2 (O2) limits the harm of a break found after the tag.

## O4 - failure - minor

**The plan cannot see drafts with `contents: read`.** GitHub's REST reference for listing releases:
"Only users with push access will receive listings for draft releases." Decision 2 has the plan
find drafts left by failed runs, but every job except the release job runs without
`contents: write` (decision 8). The plan thus treats such a version as missing, which is right,
but the deletion of the draft must happen in the release job, which is the only one that sees it.
Decision 8 should say that the release job looks for and deletes drafts of the tag before creating
one.

**Disposition: fixed.** Decision 8: the release job, the only one that sees drafts, deletes a draft
of the tag before it creates the release.

## O5 - failure - minor

**The tests run in a bare container unlike the CI's runner.** Decision 4 names only "packages from
the Debian archive". `debian:trixie` has no fonts; with platform `offscreen` Qt then lays out text
without a real font (the hitokoto-Wissen records the same for Windows: "Qt offscreen unter Windows:
keine Schriften", UI tests affected), and several UI tests depend on font metrics (`tst_s033` skips
when no page forms). The timing tests (S017 stall, benchmarks are off) also run on a container
instead of the runner they are tuned for. The story should name the test environment (a font
package such as `fonts-dejavu-core`, the googletest of trixie or the fetched one) and say what a
flaky timing test means for a release (retry versus red).

**Disposition: fixed.** Decision 4 names googletest (`libgtest-dev`) and `fonts-dejavu-core` from
trixie; a timing test that fails by chance is handled with "Re-run failed jobs", like in the CI.

## O6 - operational - minor

**Maintainer of the package and the changelog is not named.** A Debian control file needs
`Maintainer`, and every changelog entry carries a name and address (decision 6 takes "the one in
the control file"). The story leaves it open. The CPack configuration uses Daniel's private address
(`CPACK_PACKAGE_CONTACT`), the hitokoto-Wissen lists that as a gap of the CPack part (2026-11-08),
and every published package would carry it. Daniel should decide which name and address go in.

**Disposition: fixed.** Decision 5: maintainer `Daniel Fürst <daniel@fuerst.priv.at>`, as in the
`PKGBUILD` (Armin, 2026-11-12).

## O7 - design - minor

**Two Debian recipes with the same package name.** Decision 12 keeps the CPack configuration
(`CPACK_GENERATOR "DEB;TGZ"`, binary only). A `cpack` build between tags is versioned
`0.5.1+3.gabc1234`, which dpkg sorts above `0.5.1-1` (checked with `dpkg --compare-versions`), so
whoever installs it locally gets no released package until the next version, and a package without
desktop entry, icon or copyright file. Options: drop `DEB` from `CPACK_GENERATOR` in this story, or
say in the README that the published package comes from `packaging/debian` only.

**Disposition: fixed.** Decision 12: `DEB` leaves `CPACK_GENERATOR`, the `CPACK_DEBIAN_*` settings
go, `TGZ` stays for S012 (Armin, 2026-11-12); a lint checks it.

## O8 - threat - minor

**The server installs as root what only a checksum from the same release vouches for.**
Decision 11 checks the package against `SHA256SUMS` from the same release: that guards the
download, not the origin; whoever can write releases can replace both. Signing is out of scope
(decision 12), but GitHub offers a check without a key of our own: with immutable releases turned
on, publishing creates a release attestation that `gh release verify` checks, and assets cannot be
replaced afterwards. The story could ask Daniel to turn immutable releases on and name the check
for the server.

**Disposition: documented.** The notes suggest immutable releases to Daniel and name the check with
`gh release verify` for the server; decision 8 already works with them. Signing stays out of scope.

## O9 - design - minor

**The copyright file quotes the GPL instead of referring to common-licenses.** Decision 3 puts the
licence text from `LICENSE` into the copyright file. Debian Policy 12.5 asks packages under the
GPL to refer to `/usr/share/common-licenses/GPL-3` instead of quoting it, and `lintian` reports
the full text (`copyright-file-contains-full-gpl-license`). Not an error under decision 5, but an
avoidable warning in every run.

**Disposition: fixed.** Decision 3: the copyright file refers to `/usr/share/common-licenses/GPL-3`.

## O10 - failure - minor

**Removing the package "without leftovers" is not defined.** Decision 7 ends with removing the
package "without leftovers", but no scenario checks it, and it is open what counts (files under
`/usr`, the dpkg database entry after `purge`, the desktop and icon caches). As written it is not
testable; either a concrete check (`dpkg -L` before, every listed file gone after `apt-get purge`)
or drop it.

**Disposition: fixed.** Decision 7: after `apt-get purge` no file that `dpkg -L` listed for the
package is left.

## Explicitly not objecting to

- Releases as the state and catch-up of missing versions (decision 2), like the AUR in S034.
- The recipe outside the tagged source: the tags so far carry no Debian recipe, and Armin wants
  0.5.1 as the first package.
- Building with Debian's Qt 6.8.2 instead of the CI's Qt: the package is meant for trixie.
- A separate release job with `contents: write` and no other secret, draft before publish, and
  "latest" only for the highest version (decision 8).
- The versioned file name and the lookup through `releases/latest` (decision 11): Armin's choice.
- The changelog generated from Git from 0.3.0 on (decision 6): Armin's choice; the merge commits
  of main carry pull request number and title for every version so far.
- Cost: the repository is public, so the runner minutes for the extra jobs are free.
