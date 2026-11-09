#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors








set -euo pipefail

ARCH=$(uname -m)
REPOSITORY=${GITHUB_REPOSITORY:-YKW4Recomp/YKW4Recomp}
VERSION=${VERSION:-$(git -C "$(dirname "$0")/.." describe --always --dirty 2>/dev/null || echo unknown)}
export ARCH VERSION
export OUTPATH=./dist
export OUTNAME=YKW4Recomp-linux-$ARCH.AppImage

export ICON=/usr/share/icons/hicolor/256x256/apps/wimsy.png
export DESKTOP=/usr/share/applications/YKW4Recomp.desktop


if [ "${GITHUB_REF_TYPE:-}" = tag ]; then
    export UPINFO="gh-releases-zsync|${REPOSITORY%/*}|${REPOSITORY#*/}|latest|*$ARCH.AppImage.zsync"
else
    export UPINFO="gh-releases-zsync|${REPOSITORY%/*}|nightly|latest|*$ARCH.AppImage.zsync"
fi

export STRACE_MODE=0 DEPLOY_SDL=1 DEPLOY_VULKAN=1 DEPLOY_OPENGL=1






quick-sharun /usr/bin/YKW4Recomp /usr/bin/YKW4Recomp-erista /usr/bin/YKW4Recomp-mariko \
    /usr/bin/YKW4Recomp-aula /usr/bin/YKW4Recomp-3ds /usr/bin/YKW4Recomp-gnuabi /usr/bin/hactool /usr/bin/as /usr/bin/ld

# The licenses travel with the AppImage: the runtime's and those of the libraries built into it (`cmake --install` put
# them in /usr/share/licenses/YKW4Recomp), and those of the Arch packages quick-sharun took the bundled libraries
# from, with a list of those packages, their versions and licenses
appdir=${APPDIR:-$PWD/AppDir}
if [ ! -d "$appdir/lib" ]; then
    echo "quick-sharun left no $appdir/lib: where it puts the bundled libraries has changed" >&2
    exit 1
fi
if [ "$(head -c 8 "$appdir/.DirIcon" | od -An -tx1 | tr -d ' \n')" != 89504e470d0a1a0a ]; then
    echo "$appdir/.DirIcon is not a PNG: quick-sharun no longer takes ICON for it" >&2
    exit 1
fi

cp /usr/share/icons/hicolor/scalable/apps/wimsy.svg "$appdir/"
mkdir -p "$appdir/share/licenses"
cp -r /usr/share/licenses/YKW4Recomp "$appdir/share/licenses/"
mapfile -t packages < <(cd "$appdir/lib" && find . -name '*.so*' -type f | while read -r lib; do
    pacman -Qqo "/usr/lib/${lib#./}" 2>/dev/null || true
done | sort -u)
for pkg in "${packages[@]}"; do
    pacman -Qi "$pkg" | grep -E '^(Name|Version|Licenses) '
    echo
    if [ -d "/usr/share/licenses/$pkg" ]; then
        cp -r "/usr/share/licenses/$pkg" "$appdir/share/licenses/"
    fi
done >"$appdir/share/licenses/bundled-packages.txt"

quick-sharun --make-appimage




probe=$(mktemp -d)
trap 'rm -rf "$probe"' EXIT
cp "$OUTPATH/$OUTNAME" "$probe/Probe.AppImage"
check() {  
    local want=$1 out
    shift
    out=$(cd "$probe" && APPIMAGE_EXTRACT_AND_RUN=1 ./Probe.AppImage "$@" 2>&1 || true)
    if ! grep -qF "$want" <<<"$out"; then
        echo "The AppImage did not look for $want. It printed:" >&2
        echo "$out" >&2
        exit 1
    fi
}

check "$probe/Probe_code.so" -H
check "$probe/games/Other_code.so" --game="$probe/games/Other"


for program in YKW4Recomp-3ds YKW4Recomp-gnuabi hactool as ld; do
    if [ ! -e "$appdir/bin/$program" ]; then
        echo "The AppImage has no bin/$program, which the launcher runs" >&2
        exit 1
    fi
done
echo "AppImage checked: it looks for its game module next to itself or where --game says, and has the launcher"
