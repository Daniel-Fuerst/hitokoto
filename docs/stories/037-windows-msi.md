# S037 — Every version gets a Windows installer (MSI)

- **Status:** Confirmed (Armin, 2026-11-13, after the review `docs/reviews/037-windows-msi-spec.md`
  and its dispositions; ADR 0012 accepted with it) · **Order:** 38 · **Size:** L · **Depends on:**
  S030, S035, S036
- **Basis:** Armin wants a Windows installer for every version (2026-11-12, `unrefined.md`,
  "Packages and installers for every version"), as an MSI (2026-11-13; Inno Setup was considered
  before), attached to the same GitHub release as the Debian package of S036. Today every update on
  Windows means building locally, copying `hitokoto.exe` and running `windeployqt` by hand
  (2026-10-31). The tags carry no Windows recipe, and the CPack configuration of a tag makes only a
  `TGZ`, so the recipe lives beside the Debian and the AUR recipes and is applied to the tagged
  source, like S036 does. The release of 0.5.1 is published and immutable (immutable releases are on
  in the repository, S036 review O8): its assets cannot change any more.
- **Tool (checked 2026-11-13):** the image `windows-2022` of the CI (image 20260830.290.1) brings
  WiX Toolset 3.14.1, InnoSetup 6.7.1, NSIS 3.10, CMake 3.31.6 and the .NET SDKs 8 to 10. WiX v3
  (and v4) are out of community support since 2025-02-06: no further public release, also not for a
  security fix; the WiX lifecycle gives security updates to the current and the previous major
  version, now 7 (7.0.1, 2026-10-26) and 6 (6.0.2). CPack's WIX generator writes for WiX v3 by
  default and for the WiX .NET tool with `CPACK_WIX_VERSION 4` (CMake 3.30 or newer); its
  documentation shows it only with `wix` 4.0.4. The `wix` .NET tool installs with `dotnet tool
  install`; its source is under the MS-RL, its binaries under the Open Source Maintenance Fee EULA,
  whose fee applies only to users that generate revenue with it (OSMF v1.1: from US$10,000 a year);
  WiX 7 asks for the acceptance once, in a build with `-acceptEula wix7`. Since WiX 5 the `Files`
  element harvests a directory with wildcards, so a small hand-written source covers the whole
  installation without `heat`. `windeployqt` (Qt 6.8.3) bundles `vc_redist.x64.exe` as the compiler
  runtime of a release build, not the runtime's DLLs.

## Story

As someone who uses hitokoto on Windows, I want an MSI for every new version that installs it with
a Start menu entry and updates an older installation in place, so that I neither build hitokoto
nor copy Qt's files by hand.
As the maintainers, we want every version's GitHub release to carry the MSI beside the Debian
package without a manual step, and never an installer that does not install, start and uninstall
cleanly.

## Decisions

1. **Trigger and place.** Jobs of their own in `packaging.yml`, beside the Debian jobs, on the same
   events and the same commit (S036 decision 1): after every successful CI run on a push to main, on
   the commit that run passed; a pull request that changes the Windows recipe, and a manual run.
2. **One release per version, with every package.** Immutable releases cannot take an asset after
   publishing, so a version's release is made only when its Debian package and its MSI both passed:
   one release job creates it with both. The plan of S036 decides which versions to release (every
   version tag of the exact form from `first` on without a published release); both package jobs
   build exactly those versions, one matrix entry per version, no fail-fast. A version whose MSI or
   Debian package fails is not released and the run is red. It is completed without running the CI
   again (Armin, 2026-11-13): "Re-run failed jobs" in that run of "Packaging" runs the failed
   package job and the release job again, and the release job takes the other package from the first
   attempt (the attempts of a run share their artifacts; a package job hands its package on only
   after every check passed, so a failed attempt leaves none behind). The next run of "Packaging"
   and a manual run with "publish" catch it up as well. A version that cannot get an MSI for good is
   listed in the recipe as skipped for Windows, with its reason: it is then released with the Debian
   package alone, and the job summary names it. A published release without the MSI (0.5.1, and any
   version skipped for Windows) is reported and never changed, like S036 decision 2 does for the
   Debian package. So the first MSI comes with the first version tagged after this story's merge.
3. **One recipe, in the repository.** `packaging/windows` holds the recipe: the WiX source of the
   installer, the fixed upgrade code, the Qt version and the `wix` version it builds with, the list
   of files the installer must contain, the skipped versions, and a `README.md`. The recipe comes
   from the commit of decision 1 and is applied to the source of the tag; a fix to the recipe
   reaches the next released version. A recipe change alone (same version) releases nothing.
4. **Built like the Windows CI, tested first.** On `windows-2022`, with Qt `6.8.3`
   `win64_msvc2022_64` from the recipe through the same action as the CI (the CI's Windows Qt;
   aqtinstall 3.3.0 of 2025-06-02 is still the newest and cannot install Qt 6.11 or newer for
   Windows; a lint keeps the recipe's Qt version and the Windows Qt in `ci.yml` the same, review
   O9), the source of the tag is configured in Release with the version `X.Y.Z`, built with MSVC,
   and its tests run (`ctest`, platform `offscreen`, benchmarks off) as in the CI's Windows job: the
   build that is packaged is tested, not only an earlier build of the same commit, and a catch-up of
   an older version may meet a newer image. A failing test stops that version. A timing test that
   fails by chance is handled like in the CI: "Re-run failed jobs".
5. **What the installer holds.** `cmake --install` of the tag's build, then `windeployqt` (from the
   recipe's Qt) with `--release --no-translations --no-compiler-runtime --no-opengl-sw` on the
   installed `hitokoto.exe`; the C++ runtime as the DLLs of `Microsoft.VC143.CRT` from the Visual
   Studio that built it, beside the program (application-local, so no `vc_redist.x64.exe` and no
   administrator rights); `LICENSE.txt` (GPL-3.0-or-later) and a third-party notice for Qt (LGPLv3
   with its licence texts, the Qt version and where its source is) and for the Microsoft C++
   runtime. The list of files is part of the recipe; a file more or less is red, as in S036.
6. **The installer.** `hitokoto-X.Y.Z-x64.msi`, product name `hitokoto`, manufacturer `Daniel
   Fürst`, product version `X.Y.Z` (Windows Installer reads three fields: major and minor at most
   255, patch at most 65535; a version beyond that is red in the plan). Per user or per machine
   (Armin, 2026-11-13): a dialog lets the user choose "Just for me" (no administrator rights,
   `%LOCALAPPDATA%\Programs\hitokoto`, where Armin keeps his hand-made installation today) or
   "Everyone" (`C:\Program Files\hitokoto`, administrator rights). The preselection depends on the
   user's rights (Armin, 2026-11-13): an administrator finds "Everyone" preselected, a user without
   administrator rights is offered "Just for me" alone. On the command line `MSIINSTALLPERUSER=1`
   (per user, also the default without a dialog, so that a script gets the same result on every
   machine) or `ALLUSERS=1` (per machine) decides it, also without dialogs (`/qn`). In WiX: the
   scope `perUserOrMachine` (it sets `ALLUSERS=2` and `MSIINSTALLPERUSER=1`) and the scope dialog of
   `WixToolset.UI.wixext` in the version of `wix`. The dialogs are the GPL with a checkbox to accept
   it (Armin, 2026-11-13; GPL-3.0 section 9 does not require it, it is Armin's choice), the scope,
   progress and finish; an installation without dialogs needs `ACCEPT_LICENSE=yes` on the command
   line and is refused with a message otherwise. Installed: a Start menu entry `hitokoto` (for the
   user or for everyone), no desktop shortcut, an entry in "Installed apps" with the icon, version
   and an uninstall; `.md` gets hitokoto as a choice under "Open with", never as the default
   (Windows lets only the user choose that). The upgrade code is fixed in the recipe and the product
   code new for every build (ADR 0012: the upgrade code does not depend on the hosting, and a later
   change lists the old code so that the new MSI removes installations made under it), so a newer
   version replaces an older one in the same scope in place (major upgrade), and an older one over a
   newer one is refused with a message. Windows Installer looks for an older version only in the
   scope being installed: an installation per user and one per machine stand side by side, and
   `packaging/windows/README.md` says so. The user's settings survive an uninstall, as with `apt
   remove`.
7. **Built with the WiX .NET tool.** The job installs `wix` in the version of the recipe (7.0.1)
   with `dotnet tool install` and builds with `wix build`; the recipe's source harvests the
   installed tree with the `Files` element. The build job validates the MSI with `wix msi validate`;
   ICE findings accepted on purpose are listed in the recipe with their reason, any other is red
   (review O3). The OSMF EULA is accepted with `wix eula accept wix7` before the first `wix` command
   (WiX 7 runs none before, first dry run on pull request 45), and in the build with `-acceptEula
   wix7`: hitokoto earns no revenue, so no fee is due. Not CPack: CPack's WIX settings would come
   from the tag's `CMakeLists.txt`, which has none, and the WiX v3 on the image gets no security
   fixes any more.
8. **Installed, upgraded, started and uninstalled before it is released.** A second job, on a fresh
   `windows-2022` runner without Qt, gets the MSI of `X.Y.Z` and a second MSI that the build job
   made from the same files plus one marker file, with the product version `0.0.0` (never released);
   it checks that an installation without dialogs and without `ACCEPT_LICENSE=yes` is refused, then
   installs the `0.0.0` one, then the MSI of `X.Y.Z` over it (`msiexec /qn`, with a log), and checks
   that exactly one hitokoto is installed, in version `X.Y.Z`, with exactly the files of decision 5
   (so the marker file of `0.0.0` is gone, review O4) and the Start menu entry; that `hitokoto.exe
   --version` prints `hitokoto X.Y.Z` with Windows' own platform (so the platform plugin and the
   runtime were deployed; `QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES` keeps Qt's message box away;
   the program is started with its output redirected and waited for, because a GUI program started
   from PowerShell otherwise returns at once, review O10); that installing `0.0.0` again is refused;
   and that after the uninstall no file, Start menu entry or "Installed apps" entry of hitokoto is
   left. The same checks run once per user (`MSIINSTALLPERUSER=1`) and once per machine
   (`ALLUSERS=1`; the runner's user is an administrator), each without dialogs.
9. **The release job.** The release job of S036 decision 8 stays the only job with `contents:
   write`, runs no build and uses no secret beyond `GITHUB_TOKEN`; it now waits for the Debian and
   the Windows jobs and uploads, per version, `hitokoto_X.Y.Z-1_amd64.deb`, `hitokoto-X.Y.Z-x64.msi`
   (unless skipped for Windows) and one `SHA256SUMS` listing both, into the draft before publishing
   it. Everything else of S036 decision 8 stays: drafts of failed runs deleted, a published release
   never changed, only the highest version marked latest. Its name and the lints of S036 follow.
10. **The decision logic is a script with tests.** Which versions need which package, the Windows
    skips, the product version check and the list of files come from CMake scripts under
    `scripts/packaging/`, run with `cmake -P`; `ctest` cases `S037_*` cover them on all three CI
    platforms, and lints over `packaging.yml` keep the permissions, triggers and the hand-over of
    decisions 1, 2 and 9.
11. **Dry run.** A pull request that changes `packaging/windows`, `scripts/packaging` or
    `packaging.yml`, and a manual run without "publish", build, test and check the MSI for the
    highest version tag (today 0.5.1), print which versions would be released with which packages,
    and release nothing, without `contents: write`.
12. **Unsigned.** The MSI is not signed: Windows SmartScreen warns about an unknown publisher on the
    first start of the download. `packaging/windows/README.md` says so, explains how to check the
    MSI against `SHA256SUMS`, and how to build it locally.
13. **Out of scope.** Signing, winget, Microsoft Store, Chocolatey, an MSIX or an EXE bootstrapper;
    moving an installation from one scope to the other; an update check in hitokoto
    (`unrefined.md`); arm64; an installer for macOS (S012); the build of the MSI in the CI of every
    pull request; a version resource in `hitokoto.exe` (`unrefined.md`).

## Acceptance scenarios

**Scenario: a new version gets its release with both packages**
- Given the releases go up to 0.5.1
- When a pull request labelled `version:patch` is merged and its green CI tags `v0.5.2`
- Then "Packaging" builds and tests 0.5.2 for Debian and for Windows, checks both packages, and
  publishes the release `v0.5.2` with `hitokoto_0.5.2-1_amd64.deb`, `hitokoto-0.5.2-x64.msi` and
  `SHA256SUMS` listing both, marked as latest

**Scenario: installing and updating on Windows**
- Given a Windows 10 22H2 or 11 user without administrator rights and hitokoto 0.5.2 installed from
  its MSI
- When they run `hitokoto-0.5.3-x64.msi`
- Then hitokoto 0.5.3 replaces 0.5.2 under `%LOCALAPPDATA%\Programs\hitokoto`, "Installed apps"
  lists hitokoto once, in version 0.5.3, and the Start menu entry starts it
- And `.md` files offer hitokoto under "Open with"

**Scenario: for everyone, from the command line**
- Given an administrator on Windows 11
- When they run `msiexec /i hitokoto-0.5.3-x64.msi ALLUSERS=1 ACCEPT_LICENSE=yes /qn`
- Then hitokoto 0.5.3 is installed under `C:\Program Files\hitokoto` with a Start menu entry for
  every user, without a dialog
- And without `ALLUSERS=1` the same command installs it per user
- And started by double click, its dialog has "Everyone" preselected for an administrator and offers
  "Just for me" alone to a user without administrator rights

**Scenario: the licence is accepted**
- When the MSI is started by double click
- Then it shows the GPL and installs only after the checkbox to accept it is ticked
- And `msiexec /i hitokoto-0.5.3-x64.msi /qn` without `ACCEPT_LICENSE=yes` installs nothing and logs
  that the licence was not accepted

**Scenario: an older MSI over a newer installation**
- Given hitokoto 0.5.3 is installed
- When the MSI of 0.5.2 is run
- Then it refuses with a message and 0.5.3 stays installed

**Scenario: uninstalling leaves nothing behind**
- When hitokoto is uninstalled from "Installed apps"
- Then no file under `%LOCALAPPDATA%\Programs\hitokoto`, no Start menu entry and no "Open with"
  entry that the installer wrote is left (Explorer's own list of programs a user chose is not the
  installer's, review O11)

**Scenario: an installer that does not work is not released**
- Given a test fails, the installer misses or adds a file, `hitokoto.exe --version` prints another
  version or does not start, the upgrade leaves two installations, or the uninstall leaves a file
- Then the run is red and no release is created or changed for that version, also not with the
  Debian package alone

**Scenario: a release is completed by re-running the failed job**
- Given the Windows job of 0.5.2 failed by chance, its Debian package passed, and no release
  `v0.5.2` exists
- When the failed jobs of that run of "Packaging" are re-run
- Then the Windows job and the release job run again, and the release `v0.5.2` is published with
  both packages; the CI is not run again

**Scenario: a version skipped for Windows**
- Given `0.5.2` is listed in the recipe as skipped for Windows, with its reason
- When "Packaging" runs
- Then the release `v0.5.2` carries the Debian package and `SHA256SUMS` only, and the job summary
  names 0.5.2 with the reason

**Scenario: a published release stays as it is**
- Given the release `v0.5.1` was published by S036 without an MSI
- When "Packaging" runs
- Then the job summary says that `v0.5.1` has no MSI, and nothing is changed or uploaded

**Scenario: a dry run releases nothing**
- Given a pull request that changes the Windows recipe, or a manual run with "publish" off
- Then the run builds, tests, installs, upgrades, starts and uninstalls the MSI for the highest
  version tag, prints the versions it would release with their packages, and creates no release

**Scenario: the decision logic is tested**
- Then `ctest` runs the cases `S037_*` for which packages each version needs (Windows skips,
  versions released before, versions beyond the limits of Windows Installer) and for the check of
  the installer's file list, on Linux, Windows and macOS
- And the lints fail if a job other than the release job gets `contents: write`, if the release job
  does not wait for the Windows job, or if the Windows job can release on a pull request

## Notes

- Label of this story's pull request: `version:none` (nothing changes in the program); its merge
  therefore releases nothing new, and the first MSI comes with the next version tag. The dry run on
  the pull request builds and checks the MSI of 0.5.1.
- `packaging/debian/README.md` checks the download with `sha256sum -c SHA256SUMS`; with the MSI
  listed there too, a check of the Debian package alone needs `--ignore-missing`. The README
  follows, and so must a script that fetches one package (S036 decision 11; review O5).
- `packaging/windows/README.md` also says that `wix` is fetched from nuget.org in the job, pinned by
  version (review O6), that a per-user installation lies in a directory every program of the user
  can write, so "Everyone" is the choice for a protected one (review O7), and that Windows Update
  does not update the C++ runtime installed with hitokoto; it is renewed with the next version built
  on a newer image (review O8).
- WiX's dialog set `WixUI_Advanced` (`WixToolset.UI.wixext` 7.0.1) already behaves as decision 6
  asks: its `InstallScopeDlg` shows the choice between the scopes only when Windows Installer sets
  `Privileged`, preselecting what the package sets in `WixAppFolder` (here `WixPerMachineFolder`,
  "Everyone", as in WiX's own test of that set), and sends a user without it to "Just for me".
  Microsoft documents `Privileged` as set "if the user has administrator privileges"; Armin's test
  (2026-11-13, MSI of 0.5.1 from the dry run on pull request 45, double click as an administrator
  under UAC) confirmed it: welcome, the GPL with the checkbox ("Next" only after ticking it), the
  scope with "Install for all users of this machine" preselected, ready, UAC prompt after "Install",
  finish. The recipe's dialog sequence shows the licence (an RTF file made from `LICENSE`) on a page
  of its own. The same test found that choosing "Everyone" installed the files for the machine but
  the Start menu entry and the "Open with" entry for the user: Windows deletes `ALLUSERS=2` when the
  UI starts with `MSIINSTALLPERUSER=1`, and the dialog set only `MSIINSTALLPERUSER` (verbose log:
  the server got `ALLUSERS=""`). The dialog now sets `ALLUSERS` with its choice, and its texts
  (`hitokoto.wxl`) no longer promise a choice of folder (review (code) O1).
- `windeployqt` is called directly rather than through `qt_generate_deploy_app_script`: the tags'
  `CMakeLists.txt` has no deployment step, and the recipe must work on their source.
- Translations (`--no-translations`) are left out because hitokoto has no localisation yet; the
  software OpenGL renderer (`--no-opengl-sw`) because hitokoto draws with Qt Widgets only.
