#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# S036: builds the Debian package of one version in debian:trixie, against Debian's Qt.
#
#   bash packaging/debian/build-package.sh <source dir> <version> <changelog> <output dir>
#
# <source dir> holds the source of the tag v<version> (git archive, no .git needed); the recipe is
# the one next to this script (the commit the CI passed), applied to that source. The script
# configures in Release with the version given, builds, runs the tests (platform offscreen,
# benchmarks off), installs into a staging tree, adds the desktop entry (shared with the AUR
# recipe), the icon, the copyright file and the changelog, takes Depends from dpkg-shlibdeps, and
# writes <output dir>/hitokoto_<version>-1_amd64.deb and <output dir>/lintian.txt. lintian must
# report no error. SOURCE_DATE_EPOCH, if set, is the time of every file. The packages it needs
# are listed in build-depends next to it.
set -euo pipefail

if [ "$#" -ne 4 ]; then
    echo "usage: $0 <source dir> <version> <changelog> <output dir>" >&2
    exit 2
fi
src=$(realpath "$1")
version=$2
changelog=$(realpath "$3")
out=$(realpath -m "$4")
recipe=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$recipe/../.." && pwd)
script="$root/scripts/packaging/DebianPackage.cmake"
# Review O1: one desktop entry for both recipes, where the AUR recipe needs it.
desktop="$root/packaging/archlinux/hitokoto.desktop"

if ! [[ "$version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
    echo "version '$version' is not MAJOR.MINOR.PATCH" >&2
    exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build="$work/build"
stage="$work/stage"

# Decision 4: Release, the version of the tag, tests on, benchmarks and fuzzers off.
cmake -S "$src" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DHITOKOTO_VERSION="$version" \
    -DHITOKOTO_BUILD_TESTS=ON \
    -DHITOKOTO_BUILD_BENCHMARKS=OFF \
    -DHITOKOTO_BUILD_FUZZERS=OFF \
    -DHITOKOTO_WARNINGS_AS_ERRORS=OFF
cmake --build "$build"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure

# Decision 5: the files of the package.
DESTDIR="$stage" cmake --install "$build" --strip
install -Dm644 "$desktop" "$stage/usr/share/applications/hitokoto.desktop"
install -Dm644 "$src/icons/hitokoto.svg" "$stage/usr/share/icons/hicolor/scalable/apps/hitokoto.svg"
install -Dm644 "$recipe/copyright" "$stage/usr/share/doc/hitokoto/copyright"
gzip -9n < "$changelog" > "$stage/usr/share/doc/hitokoto/changelog.Debian.gz"

# Nothing else may come along (an install rule of a dependency, a test helper).
expected=$(printf '%s\n' \
    ./usr/bin/hitokoto \
    ./usr/share/applications/hitokoto.desktop \
    ./usr/share/doc/hitokoto/changelog.Debian.gz \
    ./usr/share/doc/hitokoto/copyright \
    ./usr/share/icons/hicolor/scalable/apps/hitokoto.svg)
actual=$(cd "$stage" && find . -type f -o -type l | LC_ALL=C sort)
if [ "$actual" != "$expected" ]; then
    echo "unexpected files in the package:" >&2
    diff <(echo "$expected") <(echo "$actual") >&2 || true
    exit 1
fi

find "$stage" -type d -exec chmod 755 {} +
find "$stage" -type f -exec chmod 644 {} +
chmod 755 "$stage/usr/bin/hitokoto"

# Depends from the libraries the binary links (dpkg-shlibdeps wants a debian/control to read).
mkdir -p "$work/shlibs/debian"
printf 'Source: hitokoto\n\nPackage: hitokoto\nArchitecture: amd64\n' > "$work/shlibs/debian/control"
depends=$(cd "$work/shlibs" && dpkg-shlibdeps -O -e"$stage/usr/bin/hitokoto" | sed -n 's/^shlibs:Depends=//p')
if [ -z "$depends" ]; then
    echo "dpkg-shlibdeps found no dependencies" >&2
    exit 1
fi
size=$(du -sk --apparent-size "$stage" | cut -f1)

mkdir -m 755 "$stage/DEBIAN"
cmake -DMODE=control -DCONTROL_FILE="$recipe/control" -DVERSION="$version" -DDEPENDS="$depends" \
      -DINSTALLED_SIZE="$size" -DOUTPUT="$stage/DEBIAN/control" -P "$script"
(cd "$stage" && find usr -type f -print0 | LC_ALL=C sort -z | xargs -0 md5sum) > "$stage/DEBIAN/md5sums"
chmod 644 "$stage/DEBIAN/control" "$stage/DEBIAN/md5sums"

if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    find "$stage" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
fi

mkdir -p "$out"
deb="$out/hitokoto_${version}-1_amd64.deb"
dpkg-deb --root-owner-group -Zxz --build "$stage" "$deb"
dpkg-deb --info "$deb"

# Decision 5: no lintian error; the warnings go to lintian.txt for the job summary.
set +e
lintian --fail-on error "$deb" > "$out/lintian.txt" 2>&1
status=$?
set -e
cat "$out/lintian.txt"
if [ "$status" -ne 0 ]; then
    echo "lintian reports an error (exit $status)" >&2
    exit 1
fi
