# Windows installer (MSI)

Every version of hitokoto after 0.5.1 gets an MSI for Windows 10 22H2 and 11 (x64) in the same
GitHub release as its Debian package (S037, ADR 0012). The release of a version is published only
when both packages passed their checks; a release published before S037 (0.5.1) stays without an
MSI. This directory holds the recipe, applied to the source of each version tag:

- `recipe.txt`: the Qt version and arch (the same as the Windows CI), the version of the WiX .NET
  tool, versions skipped for Windows (`skip X.Y.Z <reason>`; released with the Debian package
  alone) and validation findings accepted on purpose (`ice ICEnn <reason>`).
- `hitokoto.wxs`: the installer: the upgrade code, both scopes, the dialogs, the Start menu entry
  and the "Open with" entry for `.md`.
- `files.txt`: the files of the installation; a file more or less makes the build red. The C++
  runtime's part follows the Visual Studio of the runner image: when an image update adds or drops
  a DLL there, every MSI is red until the list follows; the build prints the installed list.
- `hitokoto.wxl`: texts of WiX's dialogs that hitokoto overrides.
- `installer-dialog.bmp`, `installer-banner.bmp`: the hitokoto logo in the dialogs, made from
  `icons/hitokoto.svg` by `scripts/generate-installer-bitmaps.sh` (ImageMagick); after a change of
  the logo, run it and commit both files.
- `third-party.txt.in`, `LGPL-3.0.txt`: the notice for Qt and the C++ runtime, and Qt's licence.
- `build-package.ps1`: builds, tests and assembles the MSI of one version.
- `install-check.ps1`: installs, upgrades, starts and uninstalls it, per user and per machine.

The decision logic is `scripts/packaging/WindowsPackage.cmake` (tests: `S037_*`), the workflow
`.github/workflows/packaging.yml`.

## Installing

Download `hitokoto-X.Y.Z-x64.msi` from the release and check it against `SHA256SUMS`, which lists
every package of the release (PowerShell):

```powershell
(Get-FileHash hitokoto-X.Y.Z-x64.msi -Algorithm SHA256).Hash.ToLower()
Select-String hitokoto-X.Y.Z-x64.msi SHA256SUMS
```

The MSI is not signed: Windows SmartScreen warns about an unknown publisher; "More info", then
"Run anyway" starts it. The installer shows the GPL to accept and installs

- "Just for me": into `%LOCALAPPDATA%\Programs\hitokoto`, without administrator rights. Every
  program of the user can write there.
- "Everyone": into `C:\Program Files\hitokoto`, with administrator rights. An administrator finds
  this choice preselected; a user without administrator rights is offered "Just for me" alone.

Without dialogs, the command line decides, and the licence must be accepted there:

```bat
msiexec /i hitokoto-X.Y.Z-x64.msi /qn ACCEPT_LICENSE=yes
msiexec /i hitokoto-X.Y.Z-x64.msi /qn ACCEPT_LICENSE=yes ALLUSERS=1
```

The first installs per user (`MSIINSTALLPERUSER=1` is the default), the second per machine. A newer
MSI replaces an older version in the same scope; an older MSI over a newer version is refused. An
installation per user and one per machine do not replace each other: they stand side by side, and
moving from one to the other means uninstalling first. The installer adds a Start menu entry and
offers hitokoto under "Open with" for `.md` files; it never makes itself the default.

What the installation contains besides `hitokoto.exe`: Qt's libraries and plugins (`windeployqt`,
without translations, the software OpenGL renderer and the shader compilers), the C++ runtime of
Microsoft Visual C++ beside the program (Windows Update does not update these copies; the next
version of hitokoto brings newer ones), `LICENSE.txt`, `LGPL-3.0.txt` and `THIRD-PARTY.txt`.

## Building locally

In a "x64 Native Tools Command Prompt for VS 2022" with PowerShell 7, Qt 6.8.3 for MSVC 2022 64-bit
and the WiX tool (`dotnet tool install --global wix --version 7.0.1`, then `wix eula accept wix7`
and `wix extension add --global WixToolset.UI.wixext/7.0.1`), from the root of a checkout with the
tags:

```bat
set QT_ROOT_DIR=C:\Dev\Qt\6.8.3\msvc2022_64
set PATH=%QT_ROOT_DIR%\bin;%PATH%
mkdir msi-work\source
git archive v0.5.1 | tar -x -C msi-work\source
pwsh packaging\windows\build-package.ps1 -Source msi-work\source -Version 0.5.1 -Out msi-work\out -WixVersion 7.0.1
```

WiX 7 runs no command before its EULA is accepted (`wix eula accept wix7`, or `-acceptEula wix7`
per build): WiX's binaries come under the Open
Source Maintenance Fee EULA, whose fee applies only to those who generate revenue with WiX
(ADR 0012).
