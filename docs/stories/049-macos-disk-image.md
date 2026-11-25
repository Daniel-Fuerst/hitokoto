# S049 — Every version gets a macOS disk image (Apple silicon)

- **Status:** In progress since 2026-11-25 (branch `packaging/macos-packaging`); Confirmed (Daniel,
  2026-11-25: "extend that to macos", with decisions 4 and 6) · **Order:** 54 · **Size:** M ·
  **Depends on:** S030, S035, S036, S037
- **Basis:** "Packaging" builds a Debian package (S036) and an MSI (S037) for every version and
  attaches both to the version's GitHub release; macOS gets nothing, although the CI builds and
  tests on `macos-15` in every pull request and S016 already gives the bundle its icon
  (`icons/hitokoto.icns`). S012 asks for a macOS `.dmg` (`macdeployqt`, signed and notarised once a
  developer certificate exists). Daniel asked on 2026-11-25 to extend the automatic packages to
  macOS, in the same way as Windows and Debian.

## Story

As someone who uses hitokoto on a Mac, I want a disk image for every new version that I can drag
into Applications, so that I neither build hitokoto nor deploy Qt by hand.
As the maintainers, we want every version's GitHub release to carry the disk image beside the
Debian package and the MSI without a manual step, and never a disk image whose program does not
start on a Mac without Qt.

## Decisions

1. **Trigger and place.** Jobs of their own in `packaging.yml`, beside the Debian and the Windows
   jobs, on the same events and the same commit (S036 decision 1, S037 decision 1).
2. **One release per version, with every package.** As S037 decision 2: the plan of S036 decides
   which versions to release; the macOS jobs build exactly those, one matrix entry per version, no
   fail-fast. A version's release waits for its DMG as for its MSI (immutable releases take no
   asset later); a failed DMG is completed with "Re-run failed jobs". A version that cannot get a
   DMG for good is listed in the recipe as skipped for macOS, with its reason, and released
   without one. A published release without the DMG (every version up to this story's merge) is
   reported and never changed. So the first DMG comes with the first version tagged after the
   merge.
3. **One recipe, in the repository.** `packaging/macos` holds the recipe: the Qt version and arch,
   the oldest macOS, the list of files of the bundle, the skipped versions, the third-party notice,
   the build and the check script, and a `README.md`. It comes from the commit of decision 1 and is
   applied to the source of the tag (S037 decision 3).
4. **Built like the macOS CI, tested first, for Apple silicon alone.** On `macos-15` (the image of
   the macOS CI, Apple silicon) with Qt `QT_VERSION` `clang_64` from the recipe through the same
   action and release of aqtinstall as the macOS CI (a lint keeps the recipe, `QT_VERSION` and the
   macOS row of `ci.yml` the same), the source of the tag is configured in Release with the version
   `X.Y.Z`, for `arm64` (Daniel, 2026-11-25: Apple silicon only; Intel Macs get nothing) and with the
   deployment target of the recipe, `13.3` (docs/stories/README.md decision 2: macOS 13 or newer;
   13.3 rather than 13.0, see "Fix of 2026-11-25"),
   built with Xcode's Clang, and its tests run (`ctest`, platform `offscreen`, benchmarks off). Qt's
   `clang_64` files hold arm64 and x86_64; every Mach-O file of the bundle is thinned to arm64. The
   build fails if any Mach-O file needs a newer macOS than the recipe's (`LC_BUILD_VERSION`), so a
   Qt that drops macOS 13 shows at once, and `LSMinimumSystemVersion` says the same to Finder.
5. **What the bundle holds.** `cmake --install` of the tag's build (`hitokoto.app`, its icon and
   `Info.plist` from `src/app/CMakeLists.txt`), then `macdeployqt` from the recipe's Qt; the image
   format plugins are removed afterwards, as in the MSI (ADR 0013: hitokoto decodes no image file,
   its icon is drawn by the icon engine of Qt SVG, which stays); `LICENSE.txt`, `LGPL-3.0.txt` and
   a third-party notice for Qt and the math font in `Contents/Resources`, so that they travel with
   the program. The list of files of the bundle is part of the recipe (a framework counts as one
   entry); a file more or less is red. No Mach-O file may refer to a library or run path outside
   the bundle and macOS (`otool -L`, `LC_RPATH`), so the DMG never needs the Qt of the build
   machine.
6. **The disk image, signed ad hoc.** `hitokoto-X.Y.Z-arm64.dmg`, a compressed (UDZO) HFS+ image
   named `hitokoto X.Y.Z`, holding `hitokoto.app` and a link to `/Applications` to drag it onto.
   The bundle keeps the identifier `dev.hitokoto.editor` and the version `X.Y.Z` of
   `src/app/CMakeLists.txt` (macOS keeps settings and permissions by the identifier). Signed ad
   hoc (`codesign --sign -`, from the inside out: Apple silicon runs no unsigned code, and
   `macdeployqt` and `lipo` break the linker's signatures), not with a Developer ID and not
   notarised (Daniel, 2026-11-25: there is no Apple developer account): macOS refuses to open a
   downloaded copy until the user allows it once, and `packaging/macos/README.md` says how. No
   secret is needed.
7. **Installed, started and removed before it is released.** A second job, on a fresh `macos-15`
   runner without Qt, gets the DMG of `X.Y.Z`, verifies and opens it, checks that it holds exactly
   `hitokoto.app` and the link to `/Applications`, copies the bundle into `/Applications` as Finder
   does, checks its ad-hoc signature and its files against the recipe, starts
   `hitokoto --version` with the platform of macOS and checks that it prints `hitokoto X.Y.Z` and
   loads its platform plugin from the bundle and no library from outside the bundle and macOS
   (dyld's own list), and that after the removal nothing of hitokoto is left in `/Applications`.
   Only then the DMG is handed on, unzipped, as the artifact `hitokoto-X.Y.Z-arm64.dmg`; the job
   `macos-handover` downloads it as the release job does, also in a dry run.
8. **The release job.** The release job of S036 and S037 stays the only job with `contents: write`;
   it now also waits for the macOS check and uploads, per version, the DMG (unless skipped for
   macOS) beside the Debian package and the MSI, and `SHA256SUMS` lists all three.
9. **The decision logic is a script with tests.** Which versions need a DMG, the macOS skips, the
   list of files, the oldest macOS and the libraries come from
   `scripts/packaging/MacosPackage.cmake`, run with `cmake -P`; `ctest` cases `S049_*` cover it and
   the recipe on all three CI platforms, and lints over `packaging.yml` keep the permissions,
   triggers and the hand-over of decisions 1, 2, 7 and 8.
10. **Dry run.** A pull request that changes `packaging/macos`, `scripts/packaging` or
    `packaging.yml`, and a manual run without "publish", build, test and check the DMG for the
    highest version tag and release nothing.
11. **Out of scope.** Signing with a Developer ID and notarisation (S012, once an Apple developer
    account exists); Intel Macs and a universal binary; Homebrew; the Mac App Store; an update
    check (`unrefined.md`); opening `.md` files from Finder ("Open With", double click): macOS
    passes them as an Apple event (`QFileOpenEvent`), which hitokoto does not handle yet, so the
    bundle declares no document types (`unrefined.md`); the build of the DMG in the CI of every pull
    request.

## Acceptance scenarios

**Scenario: a new version gets its release with every package**
- Given the releases go up to the version of this story's merge
- When a pull request labelled `version:patch` is merged and its green CI tags the next version
- Then "Packaging" builds and tests it for Debian, Windows and macOS, checks every package, and
  publishes its release with the `.deb`, the `.msi`, `hitokoto-X.Y.Z-arm64.dmg` and `SHA256SUMS`
  listing all three

**Scenario: installing on a Mac**
- Given a Mac with Apple silicon and macOS 13.3 or newer, without Qt
- When the user opens `hitokoto-X.Y.Z-arm64.dmg`, drags `hitokoto` onto `Applications` and allows
  it once under Privacy & Security
- Then hitokoto starts from Launchpad and the Dock with its icon, and its settings show version
  `X.Y.Z`

**Scenario: a disk image that does not work is not released**
- Given a test fails, the bundle misses or adds a file, a file needs a newer macOS than 13.3 or a
  library outside the bundle, or `hitokoto --version` prints another version or does not start on a
  runner without Qt
- Then the run is red and no release is created or changed for that version

**Scenario: a version skipped for macOS**
- Given `X.Y.Z` is listed in the recipe as skipped for macOS, with its reason
- When "Packaging" runs
- Then the release of `X.Y.Z` carries no DMG, and the job summary names it with the reason

**Scenario: a published release stays as it is**
- Given a release published before this story without a DMG
- When "Packaging" runs
- Then the job summary says that it has no DMG, and nothing is changed or uploaded

**Scenario: a dry run releases nothing**
- Given a pull request that changes the macOS recipe, or a manual run with "publish" off
- Then the run builds, tests, installs, starts and removes the DMG for the highest version tag and
  creates no release

**Scenario: the decision logic is tested**
- Then `ctest` runs the cases `S049_*` for which versions get a DMG (skips, releases without it),
  the file list, the oldest macOS, the libraries outside the bundle and the recipe, on Linux,
  Windows and macOS
- And the lints fail if a macOS job gets more than `contents: read`, if the release job does not
  wait for the macOS check or stops when no DMG arrived, if a DMG reaches the release job without
  the check, or if a change of `packaging/macos` starts no dry run

## Notes

- Label of this story's pull request: `version:none` (nothing changes in the program); its merge
  therefore releases nothing new, and the first DMG comes with the next version tag. The dry run on
  the pull request builds and checks the DMG of the highest tag.
- What only macOS shows, in the dry run on the pull request: which frameworks and plugins
  `macdeployqt` of Qt 6.12.0 delivers (`files.txt` is written from what `windeployqt` delivers on
  Windows and from Qt's dependencies; the build prints the real list), whether Qt 6.12.0's
  frameworks run on macOS 13.3, and whether `hitokoto --version` starts with the cocoa platform on
  the runner.
- The S037 lint `S037_WindowsInstallsAqtinstallFromAPinnedCommit` now reads only the job
  `windows-package`: the macOS job takes the release of aqtinstall the macOS CI takes, as ADR 0013
  asks for every platform but Windows.
- Qt's licence text is taken from `packaging/windows/LGPL-3.0.txt`, so that one copy serves both
  installers (`S049_TheBundleCarriesTheLicences` keeps it there).
- The release notes, which `scripts/packaging/DebianPackage.cmake` writes, still name the Debian
  package alone, as they did after S037.

## Fix of 2026-11-25

The first dry run on pull request 80 (DMG of 0.10.1) did not compile:
`src/core/src/properties.cpp` calls the floating-point `std::to_chars`, which Apple's libc++ marks
"introduced in macOS 13.3", so a build with the deployment target 13.0 refuses it. The macOS CI
never saw it, because it builds for the runner's own macOS. The recipe applies to the source of
the tags, which cannot change, so the recipe's oldest macOS is now 13.3 (March 2023, a free update
on every Mac that runs macOS 13): still "macOS 13" in the sense of docs/stories/README.md decision
2, but not 13.0 to 13.2. `S049_TheRecipeRunsOnMacos13` expects 13.3. Daniel confirmed 13.3 as
the oldest macOS on 2026-11-25, rather than keeping 13.0 through a change of the core.

The second dry run (same day, with 13.3) compiled, passed all 646 tests, deployed, thinned and
signed the bundle, and stopped at the file list: `macdeployqt` of Qt 6.12.0 brings
`QtDBus.framework` and writes no `qt.conf`, and CMake writes no `PkgInfo`. `files.txt` now lists
what the run delivered; that Qt finds the cocoa plugin without `qt.conf` is checked when
`install-check.sh` starts the program.
