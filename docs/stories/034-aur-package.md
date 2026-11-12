# S034 — The AUR package follows every version

- **Status:** In review since 2026-11-12 (CI green on Linux, Windows and macOS on pull request 40;
  the dry run of "Packaging" built, installed and started the package for v0.5.0); Confirmed
  (Armin, 2026-11-12, after the review `docs/reviews/034-aur-package-spec.md`)
  · **Order:** 35 · **Size:** S · **Depends on:** S030
- **Basis:** Daniel's Arch Linux packaging (pull request 38, `packaging/archlinux`) and his workflow
  `.github/workflows/packaging.yml` (pull request 40, branch `packaging/aur-update-on-new-version`,
  877ef80), meant to update the AUR package `hitokoto` after every new version. The AUR holds only
  the source recipe (`PKGBUILD`, `.SRCINFO`, the desktop entry); users build hitokoto themselves
  with `makepkg` or an AUR helper. The package exists and is maintained by Daniel. The code review
  of 877ef80 (2026-11-11, `docs/reviews/034-aur-package-code.md`) found that the workflow cannot
  work: it never sets `pkgver` and ignores the repository's `PKGBUILD` (O1), `makepkg` refuses to
  run as root in the container (O2), ssh does not read the `known_hosts` written (O3), and the tag
  search over the last ten commits finds old tags and fires after `version:none` merges (O4). It
  had no story (O5). Daniel asked for it to be fixed on his branch (2026-11-12, through Armin). Of
  the entry "Packages and installers for every version" in `unrefined.md`, this story takes the AUR
  part only; the Debian package for trixie and a Windows installer are decided afterwards (Armin,
  2026-11-12).

## Story

As someone who runs hitokoto on Arch Linux, I want the AUR package to offer every new version
shortly after it is merged, so that my AUR helper builds and updates hitokoto like any other
package.
As the maintainers, we want that to happen without a manual step and without the AUR ever getting
a recipe that does not build.

## Decisions

1. **Trigger.** The workflow "Packaging" runs after every completed run of the workflow "Version"
   that a push to main started and that succeeded (`workflow_run`). Tags the tag job pushes start
   no workflow themselves (they are pushed with `GITHUB_TOKEN`), so the workflow cannot listen for
   tags. It checks out the commit that run built (`github.event.workflow_run.head_sha`), not the
   newest commit of main; a manual run uses the commit it was started on.
2. **The AUR is the state; the job only catches up.** The job takes the highest version tag of the
   exact form `vMAJOR.MINOR.PATCH` reachable from that commit (the same filter as
   `scripts/version/HitokotoVersion.cmake`), and the `pkgver` the AUR package has now, read from
   its `.SRCINFO` over anonymous HTTPS. It publishes only if the tag is higher. So a `version:none`
   merge publishes nothing, a run that was missed or failed is caught up by the next one, the AUR
   is never downgraded, and several tags set by one tag-job run lead to one update, to the highest.
   If the AUR repository has no readable `.SRCINFO` with a `pkgver`, the run is red and publishes
   nothing: the package exists, and the job never creates one.
3. **One recipe, in the repository.** `packaging/archlinux` holds the only `PKGBUILD` and
   `hitokoto.desktop`; the job takes them from the commit of decision 1, so a fix to the recipe
   applies to the next published version. The job sets `pkgver` from the tag, `pkgrel=1`, and the
   SHA-256 of the GitHub tag archive in `sha256sums` (no `SKIP`), then generates `.SRCINFO`. The AUR
   repository receives exactly `PKGBUILD`, `.SRCINFO` and `hitokoto.desktop`. `.SRCINFO` leaves
   `packaging/archlinux` (it would only go stale); the `PKGBUILD` there keeps `SKIP` for the
   archive, so that a local `makepkg -si` still works, and says that the job fills in version and
   checksum. A change to the recipe alone (same version, higher `pkgrel`) is not published; it
   reaches the AUR with the next version.
4. **Built before published.** A build job, without access to the AUR key, prepares the three
   files (decision 3), builds and installs the package with `makepkg` as an unprivileged user in the
   container `archlinux:base-devel`, against Arch's own Qt, validates the desktop entry
   (`desktop-file-validate`) and checks that the installed `hitokoto --version` prints the version
   (platform `offscreen`). It hands the three files on as an artifact. If any step fails, nothing
   is published and the run is red.
5. **A separate publish job.** Only the publish job has the GitHub environment `aur`. It builds and
   runs nothing: it clones the AUR repository over SSH, reads its `pkgver` again and stops if the
   AUR is no longer below the version to publish, copies the three files from the artifact, and
   commits and pushes only if they differ, with the message `Update to X.Y.Z`. It runs in a
   concurrency group `aur` without cancelling, so two runs never push at the same time.
6. **The decision logic is a script with tests.** Which version to publish (decision 2) and the
   values written into `PKGBUILD` (decision 3) come from a CMake script under `scripts/packaging/`,
   run with `cmake -P`, like the version scripts of S030; `ctest` cases `S034_*` cover it on all
   three CI platforms. The workflow only gathers the inputs (tags, the AUR's `.SRCINFO`, the
   archive) and acts on the result.
7. **Dry run.** The workflow can be started by hand (`workflow_dispatch`) with a switch "publish",
   off by default; a pull request that changes `packaging/archlinux`, `scripts/packaging` or the
   workflow also runs it, always without publishing and without the environment. A dry run runs the
   build job of decision 4 for the highest version tag, and prints which version it would publish
   and the diff of the three files against the AUR repository.
8. **Credentials.** The SSH private key of a key pair only for this workflow, whose public key is in
   Daniel's AUR account, is a secret of the environment `aur`, which only main may use; the commit
   author name and e-mail are variables of that environment, passed to the script as environment
   variables. The AUR host key is used only if its fingerprint equals the Ed25519 fingerprint the
   AUR publishes, which is pinned in the workflow (a scanned key with another fingerprint stops the
   run). The key is written where ssh reads it for the user
   that runs ssh, named explicitly in the ssh command together with the `known_hosts` file, and
   removed at the end of the job.
9. **Checksum of the archive.** The SHA-256 is that of GitHub's archive of the tag
   (`archive/refs/tags/vX.Y.Z.tar.gz`). GitHub keeps these archives byte-for-byte stable and
   announces changes six months ahead, but it changed them once (January 2023). Every run, also one
   that publishes nothing, compares the checksum in the AUR's `PKGBUILD` with the archive of the
   AUR's `pkgver` and warns in the job summary if they differ.
10. **Package details.** `arch=('x86_64')`: only that is built and tested (the AUR is an x86-64
    repository). `Exec=hitokoto %f`: hitokoto opens one file (`main.cpp` takes one positional
    argument), so the desktop passes one.
11. **Out of scope.** Publishing a recipe change without a new version (decision 3); GitHub
    releases, the Debian package, Windows and macOS packages (`unrefined.md`, S012).

## Acceptance scenarios

**Scenario: a new version reaches the AUR**
- Given the AUR package is at 0.5.0
- When a pull request labelled `version:patch` is merged and the tag job sets `v0.5.1`
- Then the run of "Packaging" after it builds the package and pushes one commit `Update to 0.5.1`
  to the AUR repository
- And there `PKGBUILD` has `pkgver=0.5.1`, `pkgrel=1` and the SHA-256 of the archive of `v0.5.1`,
  `.SRCINFO` matches the `PKGBUILD`, and `hitokoto.desktop` equals the repository's

**Scenario: a merge without a new version publishes nothing**
- Given the AUR package is at 0.5.1 and `v0.5.1` is the highest version tag
- When a pull request labelled `version:none` is merged
- Then the run of "Packaging" is green, says that the AUR is already at 0.5.1, and pushes nothing

**Scenario: a missed version is caught up**
- Given the AUR package is at 0.5.0, and `v0.5.1` was set but its run of "Packaging" failed
- When the next run of "Packaging" starts, after a merge with or without a new version
- Then it publishes the highest version tag reachable from that commit

**Scenario: the AUR is never downgraded**
- Given the AUR package is at a version higher than every version tag reachable from main
- Then the run publishes nothing and warns that the AUR is ahead

**Scenario: no package is created**
- Given the AUR repository has no `.SRCINFO`, or one without `pkgver`
- Then the run is red, says so, and pushes nothing

**Scenario: a recipe that does not build is not published**
- Given a `PKGBUILD` that does not build against Arch's Qt, or a desktop entry that does not
  validate
- Then the run is red and the AUR repository is unchanged

**Scenario: two runs in quick succession**
- Given two runs of "Packaging" start one after the other, the first publishing 0.5.1
- Then the second waits for the first, reads the AUR again, and publishes only a higher version

**Scenario: a dry run publishes nothing**
- Given a pull request that changes `packaging/archlinux`, or a manual run with "publish" off
- Then the run builds the package, prints the version it would publish and the diff against the
  AUR repository, uses no secret, and pushes nothing

**Scenario: the decision logic is tested**
- Then `ctest` runs the cases `S034_*` for the choice of the version (higher, equal, lower, several
  tags, tags not of the exact form, no tag, no `.SRCINFO` or no `pkgver` in it) and for the values
  written into the `PKGBUILD`, on Linux, Windows and macOS

## Notes

- Before the merge Daniel creates the key pair, puts the public key into his AUR account, and sets
  up the environment `aur` (deployment branches: main) with the secret `AUR_SSH_PRIVATE_KEY` and
  the variables `AUR_NAME` and `AUR_EMAIL`. Without it the first run after the merge is red.
- The workflow only takes effect on main (`workflow_run` runs from the default branch); before the
  merge only the dry run can be exercised. The merge of this story's pull request is `version:none`,
  but it starts "Version" and then "Packaging", which publishes `v0.5.0` right away if the AUR is
  below it: that first run is to be watched.
- `packaging/archlinux/README.md` explains the recipe, the local build and how the workflow
  publishes.
