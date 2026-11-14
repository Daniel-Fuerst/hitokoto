#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# S037 decision 14: makes the pictures of the Windows installer's dialogs from icons/hitokoto.svg,
# with ImageMagick (magick, or convert of ImageMagick 6):
#   packaging/windows/installer-dialog.bmp  493x312, Welcome and Finish: the logo on its colour
#                                           in the left 164 pixels, white where the texts stand
#   packaging/windows/installer-banner.bmp  493x58, the other dialogs: white, the logo on the right
# Run it after a change of the logo and commit both files.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
svg="$root/icons/hitokoto.svg"
out="$root/packaging/windows"
colour='#393266' # the background of the logo (icons/hitokoto.svg, .cls-1)

if command -v magick >/dev/null; then im=(magick); else im=(convert); fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

"${im[@]}" -background none -density 384 "$svg" -resize 112x112 "$tmp/logo-112.png"
"${im[@]}" -background none -density 384 "$svg" -resize 44x44 "$tmp/logo-44.png"

# BMP3 with TrueColor: uncompressed 24 bits, as the dialogs of Windows Installer take it.
"${im[@]}" -size 493x312 xc:white -fill "$colour" -draw 'rectangle 0,0 163,311' \
    "$tmp/logo-112.png" -geometry +26+100 -composite \
    -alpha off -type TrueColor BMP3:"$out/installer-dialog.bmp"
"${im[@]}" -size 493x58 xc:white "$tmp/logo-44.png" -geometry +441+7 -composite \
    -alpha off -type TrueColor BMP3:"$out/installer-banner.bmp"
