# ADR 0015: The oldest supported macOS is 14.4, the oldest macOS of the Qt we ship

- **Status:** Accepted (Daniel, 2026-11-25); proposed by Claude on 2026-11-25, on Daniel's decision
  of the same day
- **Date:** 2026-11-25
- **Deciders:** Daniel

## Context

docs/stories/README.md decision 2 (2026-10-24) promises macOS 13 or newer. Until S049 nothing
checked it: the macOS CI builds for the runner's own macOS, and no package for macOS existed.

S049 builds a disk image for every version and checks every Mach-O file of the bundle against the
oldest macOS of its recipe (`LC_BUILD_VERSION`, `minos`). The dry runs on pull request 80
(2026-11-25, DMG of 0.10.1, Qt 6.12.0 `clang_64` from aqtinstall, `macos-15`) showed:

- With the deployment target 13.0 the core does not compile: `src/core/src/properties.cpp` calls
  the floating-point `std::to_chars`, which Apple's libc++ marks "introduced in macOS 13.3".
  hitokoto's own code therefore needs 13.3.
- With 13.3 the build, all 646 tests and the bundle pass, but every framework and plugin of
  Qt 6.12.0 (QtCore, QtGui, QtWidgets, QtSvg, QtDBus, `libqcocoa`, `libqmacstyle`, `libqsvgicon`)
  needs macOS 14.4. Qt 6.12 does not run on macOS 13 at all.

So the promise of decision 2 is already broken for any macOS build with `QT_VERSION`, not only
for the disk image.

## Options

1. **macOS 14.4 or newer.** The recipe ships the Qt of the CI (`QT_VERSION`, kept equal by
   `S049_TheRecipeBuildsWithTheCiQt`), the same Qt the tests run with; the oldest macOS follows
   that Qt. Macs that cannot run macOS 14 (Sonoma) lose support.
2. **Keep macOS 13 with an older Qt in the disk image.** Qt 6.8.3, the oldest Qt hitokoto supports
   (ADR 0001), still runs on macOS 13. But the open-source Qt 6.8 gets no patch release after
   6.8.3, and Qt's advisories name 6.8.3 for Qt SVG, Qt Gui and Qt Core, which the bundle ships;
   ADR 0013 moved Windows off 6.8.3 for exactly that reason. The DMG would also ship a Qt the
   other platforms' packages no longer use.
3. **Keep macOS 13 with a Qt 6.12 of our own.** Build Qt from source with the deployment target
   13.x, if Qt 6.12 still builds for it at all. Hours of build per Qt release on the runners, a
   cache to maintain, and a Qt configuration that no one else tests.

## Decision

Option 1 (Daniel, 2026-11-25): macOS 14.4 or newer.

- `packaging/macos/recipe.txt` sets `macos 14.4`; it is the deployment target of the build,
  `LSMinimumSystemVersion` of the bundle, and the bound every Mach-O file is checked against
  (`S049_TheRecipeRunsOnTheOldestMacosOfItsQt`).
- docs/stories/README.md decision 2 now names macOS 14.4 or newer, with a reference to this ADR.
- When `QT_VERSION` changes, the disk image's check names any file that needs a newer macOS; the
  recipe then follows the new Qt in the same pull request, and this ADR's number stays in the
  recipe's comment.

## Consequences

- Macs on macOS 13 get no disk image. Every Mac with Apple silicon can run macOS 14 (Sonoma), and
  14.4 is a free update on it, so on Apple silicon (the only architecture of the disk image, S049
  decision 4) no Mac is excluded for good: only those whose owners stayed on macOS 13 or 14.0 to
  14.3.
- The oldest macOS is now bound to the Qt release: a newer Qt may raise it again without a
  separate decision about the platform, only through the recipe's check and this ADR.
- Building hitokoto from source on macOS 13 against Qt 6.8 stays possible (ADR 0001); it is not
  tested by the CI.

## Reconsider if

- A user on macOS 13 asks for a disk image, and option 2 or 3 becomes worth its cost.
- Qt raises its oldest macOS again with a new `QT_VERSION`: that pull request changes the recipe
  and adds the new version here.

## Sources (checked 2026-11-25)

- Pull request 80, Packaging run of 801b473, job "macOS disk image 0.10.1": the `to_chars` error.
- Pull request 80, Packaging run of 656b430, job "macOS disk image 0.10.1": the list of files that
  need macOS 14.4.
- docs/stories/049-macos-disk-image.md, "Fix of 2026-11-25".
