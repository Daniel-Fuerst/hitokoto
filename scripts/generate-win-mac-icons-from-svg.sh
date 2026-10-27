#!/bin/bash

if [ -f "../icons/hitokoto.ico" ]; then
    rm ../icons/hitokoto.ico
fi

if [ -f "../icons/hitokoto.icns" ]; then
    rm ../icons/hitokoto.icns
fi

magick -density 256x256 -background transparent ../icons/hitokoto.svg -define icon:auto-resize -colors 256 ../icons/hitokoto.ico

magick ../icons/hitokoto.svg ../icons/hitokoto.icns
