#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# S049: builds the macOS disk image of one version on a macOS runner (Apple silicon) with Xcode's
# Clang and the Qt of the recipe (on the path through jurplel/install-qt-action, QT_ROOT_DIR set).
#
#   bash packaging/macos/build-package.sh <source> <X.Y.Z> <out> <macOS X.Y>
#
# <source> holds the source of the tag vX.Y.Z (git archive); the recipe is the one next to this
# script (the commit the CI passed), applied to that source. The script configures in Release with
# the version given for arm64 and the oldest macOS of the recipe, builds, runs the tests (platform
# offscreen, benchmarks off), installs hitokoto.app into a staging folder, adds what macdeployqt
# deploys (without image format plugins), the licence and the third-party notice, keeps arm64
# alone in every Mach-O file, signs the bundle ad hoc, checks it (files.txt, oldest macOS, no
# library outside the bundle and macOS, the version in Info.plist) and writes
# <out>/hitokoto-X.Y.Z-arm64.dmg with hitokoto.app and a link to /Applications.
# Written for the bash 3.2 of macOS as well (nothing of bash 4).
set -euo pipefail

if [ $# -ne 4 ]; then
    echo "usage: $0 <source> <X.Y.Z> <out> <macOS X.Y>" >&2
    exit 2
fi
source_dir=$(cd "$1" && pwd)
version=$2
mkdir -p "$3"
out=$(cd "$3" && pwd)
target=$4

if ! [[ "$version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
    echo "version '$version' is not MAJOR.MINOR.PATCH" >&2
    exit 1
fi
if ! [[ "$target" =~ ^[1-9][0-9]*\.[0-9]+$ ]]; then
    echo "macOS version '$target' is not X.Y" >&2
    exit 1
fi
if [ -z "${QT_ROOT_DIR:-}" ]; then
    echo "QT_ROOT_DIR is not set (install Qt first)" >&2
    exit 1
fi

recipe=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$recipe/../.." && pwd)
script="$root/scripts/packaging/MacosPackage.cmake"
work="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/hitokoto-dmg-$version"
rm -rf "$work"
build="$work/build"
installed="$work/installed"
stage="$work/stage"
mkdir -p "$build" "$installed" "$stage"

# Decision 4: Release with the version of the tag, for Apple silicon and the oldest macOS of the
# recipe, tests on, benchmarks and fuzzers off.
cmake -S "$source_dir" -B "$build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$target" \
    -DHITOKOTO_VERSION="$version" \
    -DHITOKOTO_BUILD_TESTS=ON \
    -DHITOKOTO_BUILD_BENCHMARKS=OFF \
    -DHITOKOTO_BUILD_FUZZERS=OFF \
    -DHITOKOTO_WARNINGS_AS_ERRORS=OFF
cmake --build "$build" --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure

# Decision 5: the bundle, alone; nothing else may come along (an install rule of a dependency).
cmake --install "$build" --prefix "$installed"
listing=$(cd "$installed" && find . -mindepth 1 -maxdepth 1 | sort)
if [ "$listing" != "./hitokoto.app" ]; then
    echo "cmake --install installed more or other than hitokoto.app:" >&2
    echo "$listing" >&2
    exit 1
fi
app="$stage/hitokoto.app"
ditto "$installed/hitokoto.app" "$app"

# Qt's part. macdeployqt has no switch to leave out a plugin type: the image format plugins go
# afterwards (as in the MSI, ADR 0013: hitokoto decodes no image file; its icon is drawn by the
# icon engine of Qt SVG, iconengines/libqsvgicon, which stays).
"$QT_ROOT_DIR/bin/macdeployqt" "$app" -verbose=1
rm -rf "$app/Contents/PlugIns/imageformats"

# The licence and the third-party notice (decision 5), inside the bundle, so that they travel with
# the program when it is copied to /Applications.
cp "$source_dir/LICENSE" "$app/Contents/Resources/LICENSE.txt"
# Qt's licence text is the same for both installers; the Windows recipe keeps the one copy.
cp "$recipe/../windows/LGPL-3.0.txt" "$app/Contents/Resources/LGPL-3.0.txt"
qt_version=$(basename "$(dirname "$QT_ROOT_DIR")")
cmake -DMODE=notice -DTEMPLATE="$recipe/third-party.txt.in" -DQT_VERSION="$qt_version" \
      -DOUTPUT="$app/Contents/Resources/THIRD-PARTY.txt" -P "$script"

# The oldest macOS, also for Finder (it refuses an older one with a message of its own).
plutil -replace LSMinimumSystemVersion -string "$target" "$app/Contents/Info.plist"

# Every Mach-O file of the bundle (frameworks keep theirs under Versions/A; symbolic links are
# left out).
machos="$work/machos.txt"
: > "$machos"
while IFS= read -r -d '' f; do
    case "$(file -b "$f")" in
        *Mach-O*) printf '%s\n' "$f" >> "$machos" ;;
    esac
done < <(find "$app" -type f -print0)
if [ ! -s "$machos" ]; then
    echo "no Mach-O file in $app" >&2
    exit 1
fi

# Decision 4: Apple silicon alone. Qt's clang_64 build is universal (arm64 and x86_64); its x86_64
# half would double the download for nothing.
while IFS= read -r f; do
    archs=$(lipo -archs "$f")
    if [ "$archs" != "arm64" ]; then
        lipo "$f" -thin arm64 -output "$f.thin"
        mv "$f.thin" "$f"
    fi
    archs=$(lipo -archs "$f")
    if [ "$archs" != "arm64" ]; then
        echo "${f#"$app"/} holds '$archs', not arm64 alone" >&2
        exit 1
    fi
done < "$machos"

# Decision 6: signed ad hoc, from the inside out (install_name_tool and lipo above broke the
# signatures the linker made; Apple silicon runs no unsigned code). Not notarised.
while IFS= read -r f; do
    case "$f" in
        "$app/Contents/MacOS/"*) ;;
        *) codesign --force --sign - --timestamp=none "$f" ;;
    esac
done < "$machos"
for framework in "$app"/Contents/Frameworks/*.framework; do
    codesign --force --sign - --timestamp=none "$framework"
done
codesign --force --sign - --timestamp=none "$app"
codesign --verify --deep --strict --verbose=2 "$app"

# The checks of decision 5. The files of the bundle, a framework as one entry.
bundle_files="$work/bundle-files.txt"
(cd "$app" && find . -mindepth 1 \( -name '*.framework' -prune -print \) -o \( ! -type d -print \)) |
    sed 's|^\./||' | LC_ALL=C sort > "$bundle_files"
cmake -DMODE=files -DEXPECTED_FILE="$recipe/files.txt" -DACTUAL_FILE="$bundle_files" -P "$script"

# The oldest macOS each Mach-O file needs (LC_BUILD_VERSION, minos). The awk programs read their
# whole input: an early exit would end otool with SIGPIPE, which pipefail turns into an error.
minos_file="$work/minos.txt"
: > "$minos_file"
while IFS= read -r f; do
    minos=$(otool -l "$f" | awk '$1 == "minos" && !seen { print $2; seen = 1 }')
    if [ -z "$minos" ]; then
        echo "${f#"$app"/} has no LC_BUILD_VERSION" >&2
        exit 1
    fi
    printf '%s %s\n' "$minos" "${f#"$app"/}" >> "$minos_file"
done < "$machos"
cmake -DMODE=minos -DMACOS_TARGET="$target" -DMINOS_FILE="$minos_file" -P "$script"

# The libraries and run paths every Mach-O file refers to: the bundle and macOS only.
links_file="$work/links.txt"
: > "$links_file"
while IFS= read -r f; do
    relative=${f#"$app"/}
    # otool -L prints the file itself first, and a library its own install name (otool -D) among
    # the others; that name is no reference.
    own=$(otool -D "$f" | tail -n +2)
    otool -L "$f" | tail -n +2 | awk '{ print $1 }' | while IFS= read -r library; do
        if [ "$library" != "$own" ]; then
            printf '%s %s\n' "$relative" "$library" >> "$links_file"
        fi
    done
    otool -l "$f" | awk '$1 == "cmd" && $2 == "LC_RPATH" { r = 1; next } r && $1 == "path" { print $2; r = 0 }' |
        while IFS= read -r rpath; do
            printf '%s %s\n' "$relative" "$rpath" >> "$links_file"
        done
done < "$machos"
cmake -DMODE=links -DLINKS_FILE="$links_file" -P "$script"

# The version and the identity in Info.plist.
short=$(plutil -extract CFBundleShortVersionString raw "$app/Contents/Info.plist")
bundle_version=$(plutil -extract CFBundleVersion raw "$app/Contents/Info.plist")
minimum=$(plutil -extract LSMinimumSystemVersion raw "$app/Contents/Info.plist")
if [ "$short" != "$version" ] || [ "$bundle_version" != "$version" ] || [ "$minimum" != "$target" ]; then
    echo "Info.plist: version '$short', build '$bundle_version', oldest macOS '$minimum';" \
         "expected '$version', '$version', '$target'" >&2
    exit 1
fi

# The disk image: hitokoto.app and a link to /Applications to drag it onto. hdiutil on hosted
# runners sometimes fails with "Resource busy"; it is tried again.
dmg_root="$work/dmg"
mkdir -p "$dmg_root"
ditto "$app" "$dmg_root/hitokoto.app"
ln -s /Applications "$dmg_root/Applications"
dmg="$out/hitokoto-$version-arm64.dmg"
rm -f "$dmg"
for attempt in 1 2 3 4 5; do
    if hdiutil create -volname "hitokoto $version" -srcfolder "$dmg_root" -fs HFS+ \
            -format UDZO -imagekey zlib-level=9 -ov "$dmg"; then
        break
    fi
    if [ "$attempt" = 5 ]; then
        echo "hdiutil create failed 5 times" >&2
        exit 1
    fi
    echo "hdiutil create failed (attempt $attempt); trying again" >&2
    sleep 5
done
hdiutil verify "$dmg"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "### macOS disk image $version"
        echo
        echo "Files of hitokoto.app:"
        echo
        echo '```'
        cat "$bundle_files"
        echo '```'
        echo
        echo "Oldest macOS per Mach-O file:"
        echo
        echo '```'
        cat "$minos_file"
        echo '```'
    } >> "$GITHUB_STEP_SUMMARY"
fi
ls -l "$out"
