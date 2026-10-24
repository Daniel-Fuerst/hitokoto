# S012 — Distributable builds for Linux, Windows and macOS

- **Status:** Proposed · **Order:** 13 · **Depends on:** S000 (and the licence decision)

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

- A Debian package (`.deb`) for the family's Debian machines is planned separately; its
  packaging follows the existing Debian packaging conventions and is not part of this story.
- Flatpak (KDE runtime) is a possible later addition.
