#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# S049 (decision 7): installs, starts and removes the disk image of one version on a fresh macOS
# runner without Qt, the way a user does: open the DMG, copy hitokoto.app to /Applications, start
# it, move it away.
#
#   bash packaging/macos/install-check.sh <hitokoto-X.Y.Z-arm64.dmg> <X.Y.Z>
#
# Checks that the image verifies and holds exactly hitokoto.app and the link to /Applications;
# that the copy in /Applications has a valid ad-hoc signature and exactly the files of files.txt;
# that "hitokoto --version" prints "hitokoto X.Y.Z" with the platform of macOS (cocoa), loading no
# library from outside the bundle and macOS (so the frameworks and plugins were deployed and the
# runner's Qt, if any, is not used); and that nothing of hitokoto is left in /Applications after
# the removal. Written for the bash 3.2 of macOS as well.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <dmg> <X.Y.Z>" >&2
    exit 2
fi
dmg=$1
version=$2
recipe=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$recipe/../.." && pwd)
script="$root/scripts/packaging/MacosPackage.cmake"
work="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/hitokoto-dmg-check-$version"
rm -rf "$work"
mkdir -p "$work"
installed=/Applications/hitokoto.app

fail() {
    echo "::error::$*" >&2
    exit 1
}

if [ -e "$installed" ]; then
    fail "$installed exists before the installation (not a fresh runner)"
fi

hdiutil verify "$dmg"
mount="$work/mount"
mkdir -p "$mount"
hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$mount" "$dmg"
detach() {
    hdiutil detach "$mount" -force > /dev/null 2>&1 || true
}
trap detach EXIT

# Exactly the program and the link to drag it onto (hidden files of the volume aside).
content=$(cd "$mount" && find . -mindepth 1 -maxdepth 1 ! -name '.*' | sed 's|^\./||' | LC_ALL=C sort)
expected=$(printf '%s\n' Applications hitokoto.app)
if [ "$content" != "$expected" ]; then
    fail "the disk image holds '$(echo "$content" | tr '\n' ' ')', expected 'Applications hitokoto.app'"
fi
if [ "$(readlink "$mount/Applications")" != "/Applications" ]; then
    fail "Applications in the disk image is not a link to /Applications"
fi

# Installing: the copy a drag in Finder makes.
ditto "$mount/hitokoto.app" "$installed"
detach
trap - EXIT

codesign --verify --deep --strict --verbose=2 "$installed"
signature=$(codesign -dv "$installed" 2>&1)
case "$signature" in
    *Signature=adhoc*) ;;
    *) echo "$signature" >&2; fail "hitokoto.app is not signed ad hoc" ;;
esac
listing="$work/installed-files.txt"
(cd "$installed" && find . -mindepth 1 \( -name '*.framework' -prune -print \) -o \( ! -type d -print \)) |
    sed 's|^\./||' | LC_ALL=C sort > "$listing"
cmake -DMODE=files -DEXPECTED_FILE="$recipe/files.txt" -DACTUAL_FILE="$listing" -P "$script"

# Starting: with the platform of macOS, its output redirected, waited for at most 60 s. dyld names
# every library it loads (the program is not protected by System Integrity Protection, so
# DYLD_PRINT_LIBRARIES works for it).
out="$work/version.out"
err="$work/version.err"
env -u QT_QPA_PLATFORM -u QT_PLUGIN_PATH -u DYLD_LIBRARY_PATH -u DYLD_FRAMEWORK_PATH \
    DYLD_PRINT_LIBRARIES=1 "$installed/Contents/MacOS/hitokoto" --version > "$out" 2> "$err" &
pid=$!
waited=0
while kill -0 "$pid" 2> /dev/null; do
    if [ "$waited" -ge 60 ]; then
        kill -9 "$pid" 2> /dev/null || true
        cat "$err" >&2
        fail "hitokoto --version did not end within 60 s"
    fi
    sleep 1
    waited=$((waited + 1))
done
code=0
wait "$pid" || code=$?
printed=$(cat "$out")
if [ "$code" -ne 0 ] || [ "$printed" != "hitokoto $version" ]; then
    grep -v '^dyld' "$err" >&2 || true
    fail "hitokoto --version printed '$printed' (exit code $code), expected 'hitokoto $version'"
fi
loaded="$work/loaded.txt"
# dyld of macOS 15 prints "dyld[PID]: <UUID> PATH", older ones "dyld[PID]: PATH"; a path may hold
# spaces, so only the prefix and the UUID are cut off (S049 dry run on pull request 80 read the
# UUIDs as paths).
sed -n 's/^dyld\[[0-9]*\]: //p' "$err" | sed 's/^<[0-9A-Fa-f-]*> //' | LC_ALL=C sort -u > "$loaded"
if ! grep -q "^$installed/Contents/PlugIns/platforms/libqcocoa.dylib$" "$loaded"; then
    cat "$loaded" >&2
    fail "hitokoto did not load the platform plugin of the bundle (libqcocoa.dylib)"
fi
outside=$(grep -v -e "^$installed/" -e '^/usr/lib/' -e '^/System/Library/' -e '^/Library/Apple/' \
    -e '^/System/Volumes/Preboot/Cryptexes/OS/' "$loaded" || true)
if [ -n "$outside" ]; then
    fail "hitokoto loaded libraries from outside the bundle and macOS: $(echo "$outside" | tr '\n' ' ')"
fi

# Removing: moving the bundle to the Trash leaves nothing of hitokoto in /Applications.
rm -rf "$installed"
if ls -d /Applications/hitokoto* > /dev/null 2>&1; then
    fail "something of hitokoto is left in /Applications: $(ls -d /Applications/hitokoto*)"
fi
echo "hitokoto $version: disk image installed, started and removed"
