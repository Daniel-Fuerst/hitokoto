---
mode: code
spec: none (no story; nearest are docs/stories/012-distributable-builds.md and the entry
  "Packages and installers for every version" in docs/stories/unrefined.md)
branch: packaging/aur-update-on-new-version (pull request #40)
commit_range: >-
  d6bed8e^..877ef80 (d6bed8e via pull request #38, already on main as cec1655;
  a45e92f, 877ef80 on the branch; net diff against main: .github/workflows/packaging.yml only)
reviewer: Claude (same session, no independent subagent), 2026-11-11
objections:
  - id: O1
    category: design
    severity: major
    title: The job never moves the AUR package to the new version
    disposition: fixed
    disposition_rationale: >-
      The job takes PKGBUILD and hitokoto.desktop from the checkout, sets pkgver, pkgrel=1 and
      the archive checksum (AurPackage.cmake, MODE=pkgbuild;
      S034_ThePkgbuildGetsVersionReleaseAndChecksum), and commits only if something changed, with
      the version in the message.
  - id: O2
    category: failure
    severity: major
    title: updpkgsums and makepkg refuse to run as root in the container
    disposition: fixed
    disposition_rationale: >-
      makepkg runs as the user builder (runuser); root installs the dependencies beforehand.
  - id: O3
    category: failure
    severity: major
    title: ssh reads /root/.ssh/known_hosts, the job writes $HOME/.ssh/known_hosts
    disposition: fixed
    disposition_rationale: >-
      The ssh command names the key and the known_hosts file explicitly (IdentitiesOnly,
      UserKnownHostsFile, StrictHostKeyChecking=yes); the push runs on the runner, not in the
      container.
  - id: O4
    category: design
    severity: major
    title: The tag check finds older tags and fires on version:none merges
    disposition: fixed
    disposition_rationale: >-
      No search over commits: the highest version tag reachable from the commit the Version
      run built is compared with the pkgver in the AUR (MODE=plan; S034_* cases for higher, equal,
      lower, several and inexact tags).
  - id: O5
    category: premise
    severity: major
    title: Publishing to the AUR is an undocumented project decision
    disposition: fixed
    disposition_rationale: >-
      Story S034 (confirmed by Armin, 2026-11-12) with its review (spec); no ADR: the AUR push
      is cheap to reverse and touches no code of the program. S012 and unrefined.md updated.
  - id: O6
    category: operational
    severity: minor
    title: No dry run; the first real test publishes
    disposition: fixed
    disposition_rationale: >-
      Pull requests that change the recipe and manual runs without "publish" run everything up
      to the push and show the plan and the diff in the job summary; the push only from main.
  - id: O7
    category: threat
    severity: minor
    title: The AUR key is available to every workflow on every branch
    disposition: fixed
    disposition_rationale: >-
      The secret is read only by the publish job, which uses the environment aur (deployment
      branch main); the environment itself is set up by Daniel (story, notes).
  - id: O8
    category: threat
    severity: minor
    title: The AUR host key is trusted on first use
    disposition: fixed
    disposition_rationale: >-
      The scanned Ed25519 host key is used only with the fingerprint published by Arch (pinned
      in the workflow).
  - id: O9
    category: operational
    severity: minor
    title: Two PKGBUILDs that drift apart
    disposition: fixed
    disposition_rationale: >-
      One recipe: packaging/archlinux; .SRCINFO is generated, not kept; SKIP stays only in the
      repository copy for local builds, the AUR gets the real checksum (README).
  - id: O10
    category: failure
    severity: minor
    title: Exec=hitokoto %F passes several files, hitokoto opens only the first
    disposition: fixed
    disposition_rationale: >-
      Exec=hitokoto %f (S034_TheDesktopEntryPassesOneFile).
  - id: O11
    category: failure
    severity: minor
    title: The package is never built or tested against Arch's Qt
    disposition: fixed
    disposition_rationale: >-
      The build job builds and installs the package against Arch's Qt, validates the desktop
      entry and checks hitokoto --version before anything is pushed.
---

# Code review: AUR package updated on a new version

Scope: `packaging/archlinux/` (pull request #38, on main) and `.github/workflows/packaging.yml`
(pull request #40). `version.yml` is unchanged in the net diff (a45e92f added the job there,
877ef80 moved it out again). actionlint 1.7.12 with shellcheck reports nothing for
`packaging.yml`.

## O1 - design - major

**The job never moves the AUR package to the new version.**

`packaging.yml:82-93` clones the AUR repository and runs `updpkgsums` and
`makepkg --printsrcinfo` on the PKGBUILD that is already there. Nothing sets `pkgver` to the new
tag or resets `pkgrel`, and nothing copies `packaging/archlinux/` from this repository: the
checkout in line 68 is never used. After v0.5.0 the AUR still builds 0.4.0. Once the checksums are
filled in, every later run changes nothing, and `git commit` (line 92) exits with "nothing to
commit", so the job turns red on every new version.

Expected: copy `PKGBUILD` and `hitokoto.desktop` from the checkout into the AUR clone, set
`pkgver` from the tag and `pkgrel=1`, then `updpkgsums`, `.SRCINFO`, and commit only if something
changed, with the version in the message.


**Disposition: fixed.** The job takes PKGBUILD and hitokoto.desktop from the checkout, sets pkgver,
pkgrel=1 and the archive checksum (AurPackage.cmake, MODE=pkgbuild;
S034_ThePkgbuildGetsVersionReleaseAndChecksum), and commits only if something changed, with the
version in the message.
## O2 - failure - major

**`updpkgsums` and `makepkg` refuse to run as root in the container.**

Container jobs run as root, and `archlinux:base-devel` has no other user. makepkg stops for every
action, `--printsrcinfo` included, with "Running makepkg as root is not allowed"; `updpkgsums`
calls makepkg. Arch closed the requests for a root option as "Won't implement" (bugs.archlinux.org
FS#70590, FS#67158; advice there: create a user in the container). The job fails at line 85.

Expected: `useradd -m builder`, hand the AUR clone to it and run `updpkgsums` and
`makepkg --printsrcinfo` with `runuser -u builder --`.


**Disposition: fixed.** makepkg runs as the user builder (runuser); root installs the dependencies
beforehand.
## O3 - failure - major

**ssh reads `/root/.ssh/known_hosts`, the job writes `$HOME/.ssh/known_hosts`.**

In container jobs the runner sets `HOME=/github/home` (actions/runner,
`ContainerOperationProvider.cs:312-314`). Lines 75-79 write key and `known_hosts` there. OpenSSH
expands `~` in its own paths from the passwd entry, not from `$HOME` (`misc.c`, `tilde_expand`);
checked: with `HOME=/tmp/x`, `ssh -G aur.archlinux.org` shows
`userknownhostsfile /root/.ssh/known_hosts`. The key is found (the shell expands `~` in
`core.sshCommand`), the host key is not, and with `StrictHostKeyChecking ask` and no terminal
`git clone` stops with "Host key verification failed".

Expected: name both files in the command, e.g.
`ssh -i "$HOME/.ssh/aur" -o UserKnownHostsFile="$HOME/.ssh/known_hosts" -o IdentitiesOnly=yes`.
With O2, the user that runs ssh needs the files too.


**Disposition: fixed.** The ssh command names the key and the known_hosts file explicitly
(IdentitiesOnly, UserKnownHostsFile, StrictHostKeyChecking=yes); the push runs on the runner, not in
the container.
## O4 - design - major

**The tag check finds older tags and fires on version:none merges.**

When `head_sha` carries no tag, lines 43-48 look for a `v*` tag on the last ten commits of the
history. A version:none merge has the previous version's merge among its parents, so the check
reports a "new" version that is old. Replayed on main: `cec1655` (pull request #38, no tag) finds
`v0.4.0` on `10a60bf`; `d6a5b0d` (pull request #34, version:none) finds `v0.3.1` on `6fd7b54`. The
commit message of 877ef80 promises the opposite. `v*` also matches tags that are not versions
(`v0.3.0-tulip`), which S030 excludes with `--exclude "v*[!0-9.]*"`.

Expected: no search over commits. Take the newest version tag with the rules of S030
(`git describe --tags --abbrev=0 --match "v[0-9]*.[0-9]*.[0-9]*" --exclude "v*[!0-9.]*"` on main)
and compare it with `pkgver` in the AUR's `.SRCINFO`; update only when they differ. That is
idempotent, catches up after a cancelled Version run (the tag job tags all untagged merges, so
`head_sha` need not be the tagged commit) and makes the manual dispatch safe.


**Disposition: fixed.** No search over commits: the highest version tag reachable from the commit
the Version run built is compared with the pkgver in the AUR (MODE=plan; S034_* cases for higher,
equal, lower, several and inexact tags).
## O5 - premise - major

**Publishing to the AUR is an undocumented project decision.**

No story, no ADR, no tests. The project decides as ADRs and adds dependencies only with a reason;
this adds an external account (AUR, maintained by Daniel), a container image and an SSH secret.
S012 asks for an AppImage, a Windows installer and a `.dmg`, and says the `.deb` is separate;
the AUR appears nowhere. `unrefined.md` says that package builds run in the same workflow as the
tag job, because tags pushed by it start no other workflow; `workflow_run` is a valid alternative
to that, but the entry no longer describes what the project does.

Expected: a story (or a refinement of S012) with scenarios for "new version", "version:none" and
"dispatch", an ADR for the AUR as a distribution channel, and the `unrefined.md` entry brought up
to date.


**Disposition: fixed.** Story S034 (confirmed by Armin, 2026-11-12) with its review (spec); no ADR:
the AUR push is cheap to reverse and touches no code of the program. S012 and unrefined.md updated.
## O6 - operational - minor

**No dry run; the first real test publishes.**

`workflow_run` takes effect only once the file is on main, and `workflow_dispatch` (line 11)
pushes to the AUR from whatever branch it runs on. So far nothing has run end to end (O1-O3
suggest it would not pass), and every attempt is a push to a public package.

Expected: an input `dry_run` (default true for dispatch) that runs everything up to the push and
shows the diff of `PKGBUILD` and `.SRCINFO` in the job summary; push only on main.


**Disposition: fixed.** Pull requests that change the recipe and manual runs without "publish" run
everything up to the push and show the plan and the diff in the job summary; the push only from
main.
## O7 - threat - minor

**The AUR key is available to every workflow on every branch.**

`AUR_SSH_PRIVATE_KEY` is a repository secret. Any branch pushed to this repository can carry a
changed workflow that uses it, and with it publish to the AUR in Daniel's name. AUR packages are
a known target for injected code.

Expected: put the secret in an environment (e.g. `aur`) with the deployment branch rule `main`
and use `environment: aur` in the job.


**Disposition: fixed.** The secret is read only by the publish job, which uses the environment aur
(deployment branch main); the environment itself is set up by Daniel (story, notes).
## O8 - threat - minor

**The AUR host key is trusted on first use.**

`ssh-keyscan -H aur.archlinux.org >> known_hosts` (line 79) accepts whatever answers. The AUR
publishes its fingerprints on its home page (Ed25519
`SHA256:RFzBCUItH9LZS0cKB5UE6ceAYhBD5C8GeOBip8Z11+4`).

Expected: a fixed `known_hosts` line in the workflow, or compare the scanned key with the
fingerprint before use.


**Disposition: fixed.** The scanned Ed25519 host key is used only with the fingerprint published by
Arch (pinned in the workflow).
## O9 - operational - minor

**Two PKGBUILDs that drift apart.**

The copy in `packaging/archlinux/` keeps `pkgver=0.4.0` and `sha256sums=('SKIP' …)` forever,
while the AUR copy gets real checksums. The README describes the manual update, the workflow
does something else. `SKIP` for a versioned release tarball goes against the Arch packaging
guidelines (SKIP is meant for VCS sources).

Expected: one source of truth. Either the repository copy is the template (with a placeholder
version, filled in by the workflow, see O1), or the repository keeps only the workflow and the
AUR repository holds the PKGBUILD.


**Disposition: fixed.** One recipe: packaging/archlinux; .SRCINFO is generated, not kept; SKIP stays
only in the repository copy for local builds, the AUR gets the real checksum (README).
## O10 - failure - minor

**`Exec=hitokoto %F` passes several files, hitokoto opens only the first.**

`hitokoto.desktop:6` uses `%F`; `main.cpp:79-84` opens `files.first()` and drops the rest
without a message. Selecting three files in a file manager opens one.

Expected: `%f`, so the desktop starts one hitokoto per file.


**Disposition: fixed.** Exec=hitokoto %f (S034_TheDesktopEntryPassesOneFile).
## O11 - failure - minor

**The package is never built or tested against Arch's Qt.**

The PKGBUILD has no `check()`, builds with `HITOKOTO_BUILD_TESTS=OFF`, and the CI tests Qt 6.8.3
and 6.12 only. The workflow does not build the package either (`updpkgsums` only downloads).
A break with the Qt version Arch ships shows up first at the users.

Expected: at least a `makepkg` build in the job before the push (see O6), optionally `check()`
with the core tests.


**Disposition: fixed.** The build job builds and installs the package against Arch's Qt, validates
the desktop entry and checks hitokoto --version before anything is pushed.
## Capped concerns

- `secrets.AUR_NAME` and `secrets.AUR_EMAIL` are written into the script with `${{ }}` (lines 88-89);
  pass them as `env`, like the key.
- `arch=('x86_64' 'aarch64')`: aarch64 is not built anywhere.
- The commit message "Update hitokoto" carries no version.


Disposition of the capped concerns: name and e-mail are variables of the environment `aur`, passed
as environment variables (fixed); `arch=('x86_64')` (fixed, review (spec) O8); the commit message is
`Update to X.Y.Z` (fixed).
## Explicitly not objecting to

- Moving the job out of `version.yml` into its own workflow with `workflow_run`: it leaves the
  tag job of S030 untouched, and the Version run on a push to main is started by the merge, so
  `workflow_run` fires.
- `-DHITOKOTO_VERSION="$pkgver"`: exactly what S030 provides for builds from source archives.
- `depends` match `find_package(Qt6 6.8 … Core Gui Widgets Svg)`: `qt6-base`, `qt6-svg`.
- The paths in `package()` exist in the release tarball (`hitokoto-0.4.0/icons/hitokoto.svg`,
  `hitokoto-0.4.0/LICENSE`), and the checksum of `hitokoto.desktop` matches.
- `permissions: {}` at the top and `trap 'rm -f ~/.ssh/aur' EXIT`.
- The build needs no network beyond the sources: FetchContent only runs with tests or benchmarks on.
