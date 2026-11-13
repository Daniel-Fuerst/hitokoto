# ADR 0012: The Windows installer is an MSI, built with WiX from a recipe of its own

- **Status:** Proposed by Claude on 2026-11-13, to be accepted with the confirmation of S037
- **Date:** 2026-11-13
- **Deciders:** Armin (Daniel knows of the work; his consent is not needed, Armin, 2026-11-13)

## Context

S037 gives every version a Windows installer, attached to the same GitHub release as the Debian
package of S036. Armin wants an MSI (2026-11-13). Some of its properties can only be changed later
at a cost for every user (review O1 of S037): the identity by which Windows finds an older
installation, the scope of the installation, and the tool and its licence. This ADR records them
with the options weighed; `CLAUDE.md` asks for an ADR for decisions that are expensive to reverse,
and for a justification of new build dependencies against brief section 3.

Facts as checked on 2026-11-13:

- The CI's image `windows-2022` brings WiX Toolset 3.14.1, InnoSetup 6.7.1, NSIS 3.10, CMake 3.31.6
  and the .NET SDKs 8 to 10. WiX v3 and v4 are out of community support since 2025-02-06: no further
  public release, also not for a security fix. The WiX lifecycle gives security updates to the
  current and the previous major version, now 7 (7.0.1, 2026-10-26) and 6 (6.0.2).
- WiX (copyright .NET Foundation and contributors; releases and documentation by FireGiant) comes as
  the `wix` .NET tool, installed with `dotnet tool install`. Its source is under the Microsoft
  Reciprocal License (MS-RL). Its binaries (from v6) come under the Open Source Maintenance Fee
  EULA: "The Fee applies only to Users that generate revenue by the Software. Non-revenue-generating
  use of the Software is exempt" (OSMF v1.1 adds a threshold of US$10,000 a year). WiX 7 asks for
  the acceptance of that EULA once per build, with `-acceptEula wix7`. The EULA concerns building
  the MSI, not the people who install hitokoto.
- CPack's WIX generator writes for WiX v3 by default and for the WiX .NET tool with
  `CPACK_WIX_VERSION 4` (CMake 3.30 or newer, documented with `wix` 4.0.4). Its settings come from
  the `CMakeLists.txt` of the source being packaged; the version tags carry none.
- Immutable releases are on in the repository: a published release takes no further asset.

## Options

1. **CPack WIX with the image's WiX 3.14.1.** Nothing to install, but a tool without security fixes
   on the release path, and settings that the tags do not have.
2. **CPack WIX with the WiX .NET tool (`CPACK_WIX_VERSION 4`).** Supported tool; CPack's mode is
   documented only with `wix` 4.0.4, and its settings would still have to come from outside the
   tag's `CMakeLists.txt`.
3. **A WiX source of our own, built with the WiX .NET tool.** A short `.wxs` in `packaging/windows`
   that harvests the installed files with the `Files` element (WiX 5 and newer); the recipe of the
   checked commit applies to any tag, as for the Debian package (S036).
4. **Inno Setup** (on the image). Well known and simple, but an EXE installer, not an MSI; Armin
   chose an MSI.
5. **MSIX or a zip.** MSIX needs a signature to install at all; a zip offers no Start menu entry,
   update in place or uninstall.

## Decision

Option 3, with WiX 7.0.1 (the version is a setting of the recipe), and:

1. **Upgrade code.** One upgrade code, generated once and kept in the recipe, identifies hitokoto on
   a Windows machine; every build gets a new product code, so a newer version replaces an older one
   in place (major upgrade) and an older one over a newer one is refused. The upgrade code does not
   depend on where the project is hosted: a move away from GitHub changes download addresses, not
   the upgrade code. It can still be changed: an MSI with a new code lists the old one in an
   `Upgrade` element of its own (WiX 7: `Upgrade` with `UpgradeVersion`), finds an installation made
   under the old code and removes it during the installation. The old code then stays listed for as
   long as such installations may exist.
2. **Scope.** One MSI for both scopes (WiX scope `perUserOrMachine`, which sets `ALLUSERS=2` and
   `MSIINSTALLPERUSER=1`): a dialog offers "Just for me" (`%LOCALAPPDATA%\Programs\hitokoto`, no
   administrator rights) and "Everyone" (`C:\Program Files\hitokoto`), preselected by the user's
   rights; on the command line `MSIINSTALLPERUSER=1` (per user, the default without a dialog) or
   `ALLUSERS=1` (per machine). Windows Installer looks for an older version only in the scope being
   installed, so installations in the two scopes stand side by side; moving one to the other means
   uninstalling first.
3. **Licence.** The installer shows the GPL and asks for its acceptance (Armin, 2026-11-13); an
   installation without dialogs needs `ACCEPT_LICENSE=yes` on the command line and is refused
   otherwise. GPL-3.0 section 9 does not require acceptance to receive or run hitokoto; the dialog
   is Armin's choice for the installer, not a condition of the licence.
4. **Contents.** The installed files of the tag's build, `windeployqt` without the compiler runtime,
   the C++ runtime's DLLs beside the program (application-local, no `vc_redist.x64.exe`, so a
   per-user installation needs no administrator rights), the licence and the notices for Qt and the
   C++ runtime.
5. **Unsigned** for now.

## Consequences

- A new build dependency on the release path only: the `wix` .NET tool, downloaded in the job and
  pinned by version. Nothing of WiX is shipped to users. It is replaceable: the MSI format is
  Windows' own, and option 2 or another MSI tool can build the same product with the same upgrade
  code.
- WiX publishes a major version about once a year and fixes security problems only in the current
  and the previous one, so the recipe's version is raised at least every second major version; each
  new major version may ask for its own EULA acceptance (`-acceptEula wix8`).
- No fee is due while hitokoto is developed without revenue (OSMF EULA section 1). If that ever
  changes, the fee applies to whoever generates the revenue; the EULA also allows building WiX from
  its MS-RL source.
- The application-local C++ runtime is not serviced by Windows Update; it is updated with the next
  hitokoto version built on a newer image (S037 review O8).
- Unsigned, Windows SmartScreen warns about an unknown publisher; a signature can be added later
  without breaking upgrades, because the upgrade code, not the signature, identifies the product.
- Qt for Windows is the Qt of the Windows CI (6.8.3 while aqtinstall cannot install Qt 6.11 or newer
  for Windows).

## Reconsider if

- WiX's licence or maintenance changes so that the tool no longer fits brief section 3: options 2
  (same format, same upgrade code) or 4.
- Users need installations that move between scopes, or per-user installations are blocked on the
  machines that matter: then one scope only, announced with a version that removes the other.
- A signing certificate becomes available: sign the MSI and the program (S012).
