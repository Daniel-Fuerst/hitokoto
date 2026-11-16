# ADR 0013: The Windows package uses the Qt of the other platforms and follows Qt's advisories

- **Status:** Accepted (Armin, 2026-11-16); proposed by Claude on 2026-11-16, on Armin's decision
  of the same day
- **Date:** 2026-11-16
- **Deciders:** Armin

## Context

The MSI of S037 ships Qt: Qt6Core, Qt6Gui, Qt6Widgets and Qt6Svg with the plugins `windeployqt`
picks. Until now Windows built, tested and packaged with Qt 6.8.3, the oldest Qt hitokoto supports
(ADR 0001), while Linux and macOS use `QT_VERSION` of `ci.yml` (6.12.0). The reason was the tool,
not the choice: the CI and the packaging install Qt with `jurplel/install-qt-action`, which runs
aqtinstall, and aqtinstall 3.3.0 (2025-06-02), its newest release, cannot install Qt 6.11 or newer
for Windows, because Qt moved each Windows compiler into a folder of its own ("Failed to locate XML
data").

Facts as checked on 2026-11-16:

- The open-source Qt 6.8 gets no binary patch release after 6.8.3; later 6.8 patch releases are for
  commercial licensees (ADR 0001, "follow current minors"). So Windows would keep 6.8.3 for as long
  as aqtinstall stays as it is: nothing in the project moved the Windows Qt by itself.
- Qt's advisories name 6.8.3 among the affected versions for modules the MSI ships, among them
  CVE-2025-10728 and CVE-2025-10729 (Qt SVG, `<pattern>`), CVE-2026-6210 (Qt SVG, markers),
  CVE-2026-8168 (Qt SVG, nesting depth), CVE-2025-3512 (Qt Gui, Markdown import) and CVE-2025-5455
  (Qt Core, `qDecodeDataUrl`). Qt 6.12.0 has the fixes for these.
- One advisory names 6.12.0: CVE-2026-105924 (Qt Core, `QCborValue` to JSON), fixed in 6.12.1,
  which is not released yet. hitokoto does not call that conversion.
- The aqtinstall fix is miurahr/aqtinstall pull request 1000, merged on 2026-03-24 as
  `8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b` (version 3.3.1.dev92 from git), in no release; PyPI
  still has 3.3.0. `download.qt.io` has Qt 6.12.0 for Windows under
  `windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/`, the layout that commit reads.
- `install-qt-action` v4 takes the input `aqtsource`, a pip requirement it installs instead of
  `aqtinstall==3.3.*`, and installs `py7zr==1.1.0` before it, a workaround for broken extractions on
  Windows with later py7zr versions. aqtinstall's master since 2026-06-28 requires `py7zr>=1.1.3`
  and would replace that version; the merge commit of pull request 1000 requires `py7zr>=0.22.0`
  and keeps it.

## Options

1. **Stay on Qt 6.8.3 for Windows** until an aqtinstall release has the fix. Nothing to pin; the
   MSI ships a Qt with published vulnerabilities for an unknown time.
2. **Qt 6.9 or 6.10 for Windows**, which aqtinstall 3.3.0 can still install. Their newest
   open-source patch releases, 6.9.3 (2025-09) and 6.10.3 (2026-03), are older than the advisories
   of 2026 for Qt SVG and Qt Core, and Windows would still differ from Linux and macOS.
3. **Qt's official installer** (`install-qt-action` with `use-official`). Needs a Qt account and
   its credentials as a secret in the jobs that build the release.
4. **aqtinstall from a fixed commit** that has the fix, with `QT_VERSION` on Windows too.

## Decision

Option 4 (Armin, 2026-11-16).

1. **Which Qt.** Windows builds, tests and packages with `QT_VERSION` of `ci.yml`, the Qt of Linux
   and macOS (6.12.0). The MSI ships that Qt. The job "Linux min Qt" keeps checking the oldest
   supported Qt (6.8.3, ADR 0001). The Linux packages use the Qt of their distribution (Debian's,
   Arch's) and get its security updates from there. Lints keep the Windows rows of `ci.yml` and
   `packaging/windows/recipe.txt` at `QT_VERSION` (`S037_WindowsBuildsWithQtVersion`).
2. **How it is installed.** Every call of `install-qt-action` that can run on Windows gets
   `aqtsource: git+https://github.com/miurahr/aqtinstall.git@<commit>`, the commit as a full SHA
   with the version in a comment: in `ci.yml` the env `AQT_SOURCE_WINDOWS`, passed where
   `runner.os` is Windows, in `packaging.yml` the same commit in the Windows installer's job. Linux
   and macOS keep the action's aqtinstall release. The commit is
   `8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b`, the merge of pull request 1000: the smallest step
   from 3.3.0 that has the fix, and it keeps the action's `py7zr` 1.1.0. This is a change of pinning
   of a tool the release path already uses, not a new dependency: nothing of aqtinstall is shipped,
   and a commit is fixed more firmly than the floating `3.3.*` of today
   (`S037_WindowsInstallsAqtinstallFromAPinnedCommit`).
3. **Back to a release.** As soon as an aqtinstall release has the fix, the Windows calls take that
   exact version (`aqtversion: ==X.Y.Z`) instead of the commit.
4. **Advisories.** A Qt security advisory for a module the MSI ships (Qt Core, Gui, Widgets, Svg,
   and the plugins in `iconengines`, `platforms` and `styles`) is looked at before the next
   release; if it names the Qt of the MSI, Windows moves to a version with the fix before that
   release, together with `QT_VERSION`. Two subscriptions bring the news:
   - the mailing list "announce" of the Qt Project (`lists.qt-project.org/listinfo/announce`), which
     carries Qt's releases and its security advisories;
   - "Watch → Custom → Releases" on `miurahr/aqtinstall`, for the release that ends point 2.
5. **Fewer plugins.** The MSI ships no image format plugin (`windeployqt --skip-plugin-types` with
   `imageformats`). hitokoto opens no image file: pasted HTML loads no resources (ADR 0011), a table
   cell shows an image as its characters (S018), and the only image it draws is its icon, an SVG
   that `QIcon` draws through the icon engine of Qt SVG (`iconengines/qsvgicon`), which stays. Qt's
   own images (the standard icons of the styles) are PNGs, whose reader is part of Qt Gui
   (`S037_TheInstallerShipsNoImageFormatPlugins`).

## Consequences

- Windows and the MSI follow Qt's current release like Linux and macOS; a change of `QT_VERSION`
  reaches all three platforms in one pull request and goes through the CI and a dry run of the
  packaging before it reaches a release.
- A commit of aqtinstall's master is not a release: it saw the project's own tests, not a release
  process. The CI shows on every pull request whether it still installs Qt; the cache key of
  `install-qt-action` contains `aqtsource`, so the first run after a change installs afresh.
- The first MSI with Qt 6.12.0 needs the dry run on its pull request to confirm the file list:
  `windeployqt` of another Qt may deliver other files.
- Should a later version of hitokoto need images (brief, "planned later"), the plugins it needs come
  back with that story, each with its reason.
- Watching advisories is a manual step. It is cheap (low-traffic list, a few advisories a year for
  the shipped modules) and it is the only step that notices a vulnerability between two Qt
  releases.

## Reconsider if

- aqtinstall stays without a release for long, its master stops installing Qt for Windows, or the
  action drops `aqtsource`: option 3 with a secret, or a Qt built in the project.
- Qt's official binaries become installable without an account.
- The MSI starts to ship further Qt modules: they join the list in point 4.

## Sources (checked 2026-11-16)

- aqtinstall pull request 1000: https://github.com/miurahr/aqtinstall/pull/1000 ; releases:
  https://pypi.org/project/aqtinstall/
- install-qt-action inputs: https://github.com/jurplel/install-qt-action
- Qt for Windows on download.qt.io:
  https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/
- Known vulnerabilities in Qt: https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products
- Qt Project mailing list "announce": https://lists.qt-project.org/listinfo/announce
