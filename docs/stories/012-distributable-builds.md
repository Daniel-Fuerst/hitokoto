# S012 — Distributable builds for Linux, Windows and macOS

- **Status:** Proposed · **Order:** 41 · **Depends on:** S000 (and the licence decision)

## Story

As a user, I want to install hitokoto like any other app on my system, so that I can use it
without building it.

## Acceptance scenarios

**Scenario: one artefact per platform from CI**
- Given a tagged release
- When the release workflow runs
- Then it produces a Windows installer or zip (`windeployqt`), a macOS `.dmg` (`macdeployqt`,
  signed and notarised once a developer certificate exists) and a Linux AppImage

**Scenario: documented**
- Then `README.md` explains how to produce each artefact locally

**Scenario: licences shipped**
- Then each artefact contains hitokoto's licence and the LGPLv3 notice and source offer for Qt

## Notes

- S034 keeps the AUR package (a source recipe; users build hitokoto themselves) up to date with
  every version. A Debian package (`.deb`) for trixie built by CI belongs to this project like any
  vendor's package (Armin, 2026-10-28); S036 builds it for every version and attaches it to the
  version's GitHub release, from its own recipe in `packaging/debian`, and drops `DEB` from the
  CPack configuration in `CMakeLists.txt`; `TGZ` is left to this story.
- S037 builds a Windows installer (MSI, per user, unsigned) for every version from its own recipe in
  `packaging/windows` and attaches it to the same release as the Debian package; for Windows this
  story keeps only a zip, if one is still wanted, and signing.
- Flatpak (KDE runtime) is a possible later addition.
