---
mode: spec
spec: docs/stories/037-windows-msi.md
reviewer: Advocatus Diaboli (Claude, in the session that drafted the story; not an independent
  subagent)
date: 2026-11-13
objections:
  - id: O1
    category: design
    severity: major
    title: Decisions that are expensive to reverse are taken in a story, not in an ADR
    disposition: fixed
    disposition_rationale: >-
      ADR 0012 (Proposed, accepted with the confirmation of S037; Armin, 2026-11-13, without
      Daniel's consent, who knows of the work) records the tool, the upgrade code, the scope and the
      licence dialog with the options weighed. The upgrade code is not fixed for good: a later MSI
      with a new code lists the old one and removes installations made under it.
  - id: O2
    category: design
    severity: major
    title: A red Windows job now holds back the Debian package of the same version
    disposition: fixed
    disposition_rationale: >-
      Armin (2026-11-13): the version is final with the tag; a release that waits for both packages
      is fine if it is completed by re-running only the failed job. Decision 2 and a scenario say
      so: "Re-run failed jobs" in the run of Packaging runs the failed package job and the release
      job again, without the CI; the next run and a manual run with publish catch up as well.
  - id: O3
    category: failure
    severity: minor
    title: The per-user installation is stated as a result, not as the means that gets it
    disposition: fixed
    disposition_rationale: >-
      WiX's scope perUserOrMachine sets ALLUSERS=2 and MSIINSTALLPERUSER=1 (decision 6);
      decision 7: the build job validates the MSI with wix msi validate, ICE findings accepted
      on purpose are listed in the recipe with their reason (Armin, 2026-11-13).
  - id: O4
    category: failure
    severity: minor
    title: The upgrade test installs identical files and cannot see an old file left behind
    disposition: fixed
    disposition_rationale: >-
      Decision 8: the 0.0.0 MSI carries one marker file more, which must be gone after the
      upgrade (Armin, 2026-11-13).
  - id: O5
    category: failure
    severity: minor
    title: One SHA256SUMS for both packages breaks a strict check of one download
    disposition: fixed
    disposition_rationale: >-
      Notes: packaging/debian/README.md and a script that fetches one package check with
      sha256sum -c --ignore-missing (Armin, 2026-11-13).
  - id: O6
    category: threat
    severity: minor
    title: The wix tool comes from nuget.org at build time, checked by version only
    disposition: documented
    disposition_rationale: >-
      packaging/windows/README.md says that wix comes from nuget.org in the job, pinned by
      version (Armin, 2026-11-13).
  - id: O7
    category: threat
    severity: minor
    title: A per-user installation lies in a directory every process of the user can write
    disposition: documented
    disposition_rationale: >-
      The installer now also offers Everyone (decision 6, Armin); packaging/windows/README.md
      names it as the choice for a protected installation.
  - id: O8
    category: operational
    severity: minor
    title: The application-local C++ runtime is never serviced by Windows Update
    disposition: documented
    disposition_rationale: >-
      packaging/windows/README.md says that Windows Update does not update the runtime
      installed with hitokoto; it is renewed with the next version built on a newer image
      (Armin, 2026-11-13).
  - id: O9
    category: operational
    severity: minor
    title: Windows users get Qt 6.8.3 for as long as aqtinstall cannot install a newer Qt
    disposition: fixed
    disposition_rationale: >-
      Decision 4: a lint keeps the recipe's Qt version and the Windows Qt in ci.yml the same
      (Armin, 2026-11-13).
  - id: O10
    category: failure
    severity: minor
    title: The start check depends on a desktop session of the runner
    disposition: fixed
    disposition_rationale: >-
      Decision 8: the program is started with redirected output and waited for, with
      QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES set (Armin, 2026-11-13).
  - id: O11
    category: failure
    severity: minor
    title: "\"No Open with entry left\" is not fully in the installer's hands"
    disposition: fixed
    disposition_rationale: >-
      The scenario checks only the Open with entries the installer wrote (Armin, 2026-11-13).
  - id: O12
    category: cost
    severity: minor
    title: A Windows build and test per version delays every release
    disposition: resolved
    disposition_rationale: >-
      Armin (2026-11-13): the installer needs no benchmark or time budget; a slow Windows run
      is accepted.
---

# Review (spec) of S037 — Every version gets a Windows installer (MSI)

## O1 - design - major

Decisions 6 and 7 fix three things that cannot be taken back cheaply: the upgrade code ("fixed for
good"), the installation scope (per user) and the tool (WiX 7 under the OSMF EULA). CPack's
documentation of `CPACK_WIX_INSTALL_SCOPE` says that "installers with different scopes cannot be
cleanly updated or replaced by one another", so a later move to per machine forces every user to
uninstall first; a new upgrade code does the same. `CLAUDE.md` asks to "ask before decisions that
are expensive to reverse … record them as ADRs in `docs/adr/`", and new build dependencies need a
justification against brief section 3. The story carries the reasons only in its basis and in
decision 7; there is no ADR, and the alternatives (per machine, per user or machine, CPack with
WiX 3.14.1, Inno Setup, WiX 6) are not weighed in one place.

## O2 - design - major

Decision 2 releases a version only when its Debian package and its MSI both passed. S036
decision 2 promised that "a failing version leaves the others alone"; now a failing Windows job
(a timing test, Defender, a runner problem: the knowledge of this project has several Windows-only
reds, e.g. `tst_s017` and `startup_p95`) leaves the Debian package of the same version unreleased,
and Armin's server, which only reads `releases/latest`, does not get it. The story chooses this
because an immutable release takes no asset later, but it does not weigh the other way: release
the Debian package at once and treat a missing MSI like the skip list (that version then never
gets an MSI, the next one does). Which harm is smaller is Armin's call; the story should state it
as his decision.

## O3 - failure - minor

Decision 6 says "per user, without administrator rights, into `%LOCALAPPDATA%\Programs\hitokoto`"
but not how. With Windows Installer that needs the single-package authoring (`ALLUSERS=2` and
`MSIINSTALLPERUSER=1`, or WiX's per-user scope with the program folder redirected), and components
in the user's profile want a key path under `HKEY_CURRENT_USER` (ICE38, ICE64, ICE91); CPack's
documentation of the per-user scope mentions exactly these HKCU key paths. Files harvested with the
`Files` element have file key paths. The story does not say whether the MSI is validated
(`wix msi validate`) and which ICE findings are accepted, so a wrong mechanism would show only on
the first user's machine (for example a prompt for administrator rights).

## O4 - failure - minor

Decision 8 upgrades from an MSI "made from the same files with the product version `0.0.0`".
Identical files cannot show whether the upgrade really replaces them: `hitokoto.exe` has no version
resource (decision 13), and Windows Installer compares unversioned files by date. Whether an old
file survives depends on when the major upgrade removes the old product (WiX's default removes it
before installing, which is safe; another schedule would not be), and the test passes either way,
since `--version` of the identical exe prints `X.Y.Z` in both cases. A test file that differs
between the two MSIs (or a different exe) would make the check meaningful.

## O5 - failure - minor

Decision 9 writes one `SHA256SUMS` listing the `.deb` and the MSI. `packaging/debian/README.md`
line 31 tells users `sha256sum -c SHA256SUMS`, which fails when only the `.deb` was downloaded; the
story's note fixes the README with `--ignore-missing`, but S036 decision 11 has Armin's server
"check it against `SHA256SUMS`", and a server script written to that wording breaks with the first
release that carries an MSI. The story should say that the server must ignore missing files, or
keep one checksum file per package.

## O6 - threat - minor

Decision 7 installs `wix` 7.0.1 with `dotnet tool install` in the job that builds the release.
The version is pinned, the content is not: nuget.org packages are signed, but the story neither
checks the package hash nor the signature. Everything else in the job comes from the image or from
actions pinned by major version, so this is not worse than the rest of `packaging.yml`, but it is
a new download into a job whose artefact is released as is.

## O7 - threat - minor

A per-user installation lies under `%LOCALAPPDATA%`, which every process of the user can write:
anything that runs as the user can replace `hitokoto.exe` or a Qt DLL without a prompt, and the
next start runs it. A per-machine installation under `Program Files` is protected by the
administrator boundary. Unsigned (decision 12), a replaced file is also not detectable by a
signature. This is common for per-user apps (browsers and editors install the same way), but the
trade-off is not stated.

## O8 - operational - minor

Decision 5 deploys the C++ runtime as DLLs beside the program. Microsoft documents central
deployment (the redistributable) as the recommended way because only that one is serviced by
Windows Update; an application-local copy gets a security fix only when a new hitokoto version is
built on an image with a newer Visual Studio. The choice is right for "no administrator rights",
but the consequence belongs in the README.

## O9 - operational - minor

Decision 4 takes Qt 6.8.3 because aqtinstall 3.3.0 (2025-06-02, still the newest) cannot install
Qt 6.11 or newer for Windows. The MSI therefore ships a Qt that the project does not choose but is
stuck with, while Linux and macOS build with 6.12; there is no trigger to move on once aqtinstall
(or the CI's Windows Qt) changes. The recipe's Qt version and the CI's Windows Qt should move
together, and the story could say so.

## O10 - failure - minor

Decision 8 checks `hitokoto.exe --version` "with Windows' own platform". The program constructs a
`QApplication` before parsing (`src/app/main.cpp` line 44), so this loads `qwindows.dll`
on the runner's session; GitHub's Windows runners provide one, but a change there would make the
check red for reasons outside hitokoto. Qt shows a message box instead of printing unless a console
is attached, the handles are redirected, or `QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES` is set
(`qcommandlineparser.cpp`, Qt 6.8.3, lines 523–532); the story names the variable, but a GUI
program started from PowerShell without captured output also returns before it exits.

## O11 - failure - minor

The scenario "uninstalling leaves nothing behind" includes "no 'Open with' entry". The installer
removes its own `OpenWithProgids` value and its ProgID; Explorer also records programs a user
actually chose in its own per-user lists, which the installer did not write and does not remove.
The scenario should say "no entry the installer wrote", or the check may stay red after a manual
test.

## O12 - cost - minor

Decision 4 builds and tests the tag on Windows per version, and decision 8 adds a second runner.
In the knowledge of this project the Windows CI job is the slowest; with decision 2 every release,
including the Debian package that Armin's server waits for, now waits for it too. Running the
tests again is justified (another build, maybe another image), but the story gives no time budget
and no measurement, so a slow Windows run is not noticed until it hurts.

## Explicitly not objecting to

- An MSI rather than Inno Setup: Armin's choice (2026-11-13), and Windows Installer's upgrade and
  uninstall handling is what decision 6 needs.
- Leaving CPack out: the tags' `CMakeLists.txt` has no WIX settings, and S036 already applies a
  recipe of the checked commit to the tagged source.
- WiX 7 over the image's WiX 3.14.1: v3 gets no public security fixes since 2025-02-06.
- Leaving 0.5.1 without an MSI: its release is immutable, and the first MSI comes with the next
  version.
- Unsigned for now: a certificate is a cost and an identity question outside this story.
- `--no-translations` and `--no-opengl-sw`: hitokoto has no localisation and draws with Widgets.
