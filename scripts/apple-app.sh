#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors









set -eu
[ $# -ge 3 ] || { sed -n '4,13p' "$0"; exit 2; }
kind=$1 prefix=$2 out=$3 moltenvk=${4:-}
src=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# the file or folder in the MoltenVK release that ends with $1, and its license beside the app's other ones ($2)
moltenvk_part() {
    found=$(find "$moltenvk" -path "*/dynamic/*$1" | head -1)
    [ -n "$found" ] || { echo "no $1 in $moltenvk" >&2; exit 1; }
    license=$(find "$moltenvk" -name LICENSE | head -1)
    if [ -n "$license" ]; then
        mkdir -p "$2/licenses/YKW4Recomp/MoltenVK"
        cp "$license" "$2/licenses/YKW4Recomp/MoltenVK/LICENSE"
    fi
}
case $kind in
macos)
    app=$out/YKW4Recomp.app
    rm -rf "$app"
    mkdir -p "$app/Contents/Resources"

    cp -R "$prefix/bin" "$app/Contents/MacOS"
    cp -R "$prefix/share" "$app/Contents/share"
    cp "$src/dist/apple/Info-macOS.plist" "$app/Contents/Info.plist"
    set=$work/YKW4Recomp.iconset
    mkdir "$set"
    for size in 16 32 128 256 512; do
        sips -z $size $size "$src/dist/icons/hicolor/256x256/apps/wimsy.png" --out "$set/icon_${size}x${size}.png" >/dev/null
        retina=$((size * 2))
        sips -z $retina $retina "$src/dist/icons/hicolor/256x256/apps/wimsy.png" --out "$set/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$set" -o "$app/Contents/Resources/YKW4Recomp.icns"
    if [ -n "$moltenvk" ]; then
        moltenvk_part dylib/macOS/libMoltenVK.dylib "$app/Contents/share"
        mkdir -p "$app/Contents/Frameworks"
        cp "$found" "$app/Contents/Frameworks/"
    fi

    codesign --force --deep --sign - "$app"
    echo "$app"
    ;;
ios)
    [ -d "$prefix/YKW4Recomp.app" ] || { echo "no YKW4Recomp.app in $prefix: install the ios preset's build there" >&2; exit 1; }
    app=$work/Payload/YKW4Recomp.app
    mkdir -p "$work/Payload"
    cp -R "$prefix/YKW4Recomp.app" "$app"
    cp "$src/dist/icons/hicolor/256x256/apps/wimsy.png" "$app/AppIcon.png"
    # the licenses go with the binaries, as everywhere (CMakeLists.txt)
    cp -R "$prefix/share" "$app/share"
    if [ -n "$moltenvk" ]; then
        moltenvk_part ios-arm64/MoltenVK.framework "$app/share"
        mkdir -p "$app/Frameworks"
        cp -R "$found" "$app/Frameworks/"
    fi
    plutil -lint "$app/Info.plist" >/dev/null
    rm -f "$out/YKW4Recomp.ipa"
    (cd "$work" && zip -qry "$out/YKW4Recomp.ipa" Payload)
    echo "$out/YKW4Recomp.ipa"
    ;;
*)
    echo "macos or ios, not $kind" >&2
    exit 2
    ;;
esac
