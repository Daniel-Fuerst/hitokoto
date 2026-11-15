#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# S036 (decision 7): installs the Debian package in a fresh debian:trixie, starts it, validates the
# desktop entry, and checks that purging it leaves none of its files behind.
#
#   bash packaging/debian/install-check.sh <package file> <version>
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <package file> <version>" >&2
    exit 2
fi
deb=$(realpath "$1")
version=$2

export DEBIAN_FRONTEND=noninteractive
apt-get update -q
# The package first and alone, so that a missing dependency fails here.
apt-get install -y -q --no-install-recommends "$deb"
apt-get install -y -q --no-install-recommends desktop-file-utils

printed=$(QT_QPA_PLATFORM=offscreen hitokoto --version)
if [ "$printed" != "hitokoto $version" ]; then
    echo "hitokoto --version printed '$printed', expected 'hitokoto $version'" >&2
    exit 1
fi
desktop-file-validate /usr/share/applications/hitokoto.desktop

files=$(dpkg-query -L hitokoto)
apt-get purge -y -q hitokoto
left=""
while IFS= read -r path; do
    if [ -f "$path" ] || [ -L "$path" ]; then
        left="$left $path"
    fi
done <<< "$files"
if [ -n "$left" ]; then
    echo "files left after apt-get purge:$left" >&2
    exit 1
fi
echo "hitokoto $version installed, started, validated and purged"
