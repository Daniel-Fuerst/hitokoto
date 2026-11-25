# macOS disk image (DMG)

Every version of hitokoto released after this recipe's merge gets a disk image for macOS 13 or
newer on Apple silicon (arm64) in the same GitHub release as its Debian package and its MSI
(S049). The release of a version is published only when every package passed its checks; a
release published before (0.5.1 up to the version of the merge) stays without a disk image. This
directory holds the recipe, applied to the source of each version tag:

- `recipe.txt`: the Qt version and arch (the same as the macOS CI: `QT_VERSION` of `ci.yml`), the
  oldest macOS the disk image runs on, and versions skipped for macOS (`skip X.Y.Z <reason>`;
  released without a disk image).
- `files.txt`: the files of `hitokoto.app`, a framework as one entry; a file more or less makes the
  build red. The build prints the bundle's list in the job summary.
- `third-party.txt.in`: the notice for Qt and the math font; Qt's licence text is the one of the
  Windows recipe (`../windows/LGPL-3.0.txt`).
- `build-package.sh`: builds, tests and assembles the disk image of one version.
- `install-check.sh`: installs, starts and removes it on a fresh runner.

The decision logic is `scripts/packaging/MacosPackage.cmake` (tests: `S049_*`), the workflow
`.github/workflows/packaging.yml`.

## Installing

Download `hitokoto-X.Y.Z-arm64.dmg` from the release and check it against `SHA256SUMS`, which lists
every package of the release, so files not downloaded are left out:

```sh
shasum -a 256 -c --ignore-missing SHA256SUMS
```

Open the disk image and drag `hitokoto` onto `Applications`.

The program is signed ad hoc, not with an Apple Developer ID, and not notarised: there is no Apple
developer account behind hitokoto. macOS therefore refuses to open a downloaded copy at first
("Apple could not verify …"). Either open it once through System Settings › Privacy & Security,
"Open Anyway" (macOS 15 and newer; on macOS 13 and 14 also Control-click › Open in Finder), or
remove the quarantine mark the browser set, after checking the download as above:

```sh
xattr -dr com.apple.quarantine /Applications/hitokoto.app
```

A newer version replaces an older one by dragging it onto `Applications` again ("Replace"). To
remove hitokoto, move `hitokoto` from `Applications` to the Trash; its settings stay, as with
`apt remove`.

What the bundle contains besides the program: Qt's frameworks and plugins (`macdeployqt`, without
the image format plugins: hitokoto opens no image, and its icon is drawn by the SVG icon engine),
and in `Contents/Resources` `LICENSE.txt`, `LGPL-3.0.txt` and `THIRD-PARTY.txt`. Every file holds
arm64 code alone; Intel Macs are not supported.

## Building locally

On a Mac with Apple silicon, Xcode's command line tools, CMake and Qt 6.12.0 for macOS (for example
from the Qt online installer or `aqt install-qt mac desktop 6.12.0 clang_64`), from the root of a
checkout with the tags:

```sh
export QT_ROOT_DIR="$HOME/Qt/6.12.0/macos"
export PATH="$QT_ROOT_DIR/bin:$PATH"
mkdir -p dmg-work/source
git archive v0.5.1 | tar -x -C dmg-work/source
bash packaging/macos/build-package.sh dmg-work/source 0.5.1 dmg-work/out 13.0
```
