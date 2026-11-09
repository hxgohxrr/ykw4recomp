#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors









set -euo pipefail

if [ $# -ne 1 ] || [ ! -f "$1" ]; then
    echo "usage: $0 PACKAGED_YKW4Recomp" >&2
    exit 2
fi
runtime=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
exe=
case $runtime in
    *.exe) exe=.exe ;;

    *.AppImage) export APPIMAGE_EXTRACT_AND_RUN=1 ;;
esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"



export XDG_DATA_HOME="$work/data" XDG_CONFIG_HOME="$work/config" XDG_STATE_HOME="$work/state" XDG_CACHE_HOME="$work/cache"
data=$XDG_DATA_HOME/YKW4Recomp
config=$XDG_CONFIG_HOME/YKW4Recomp/games
state=$XDG_STATE_HOME/YKW4Recomp/games
if [ -n "$exe" ]; then
    APPDATA=$(cygpath -w "$work/data")
    export APPDATA
    config=$data/config/games
    state=$data/state/games
fi

failed=0
pass() { echo "ok: $1"; }
fail() {
    echo "FAILED: $1" >&2
    shift
    printf '%s\n' "$@" | sed 's/^/    /' >&2
    failed=1
}

run() {
    status=0
    out=$(timeout 300 "$@" 2>&1) || status=$?
}


put() { printf '%b' "$3" | dd of="$1" bs=1 seek=$(($2)) conv=notrunc status=none; }
word() { put "$1" "$2" "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' $(($3 & 255)) $(($3 >> 8 & 255)) $(($3 >> 16 & 255)) $(($3 >> 24)))"; }
head -c 512 /dev/zero >smoke.nro
word smoke.nro 0x00 0x14000020  
put smoke.nro 0x10 NRO0
word smoke.nro 0x18 0x200
for field in 0x20:0 0x24:0x100 0x28:0x100 0x2c:0x80 0x30:0x180 0x34:0x80; do  
    word smoke.nro "${field%:*}" "${field#*:}"
done
word smoke.nro 0x80 0x10000400  
word smoke.nro 0x84 0xd2800221  
word smoke.nro 0x88 0xd40004e1  
word smoke.nro 0x8c 0xd40000e1  
word smoke.nro 0x90 0xd4200000  
put smoke.nro 0x100 'NX runtime smoke\n'

for mode in "" -x -r; do
    run "$runtime" ${mode:+"$mode"} --help
    if [ "$status" -eq 0 ] && grep -q '^Usage: YKW4Recomp' <<<"$out"; then
        pass "YKW4Recomp${mode:+ $mode} --help"
    else
        fail "YKW4Recomp${mode:+ $mode} --help exited $status" "$out"
    fi
done


game=$data/games/Smoke
run "$runtime" -r smoke.nro Smoke --jobs 2
if [ "$status" -eq 0 ] && [ -f "$game/Smoke$exe" ] && [ -f "$game/Smoke.abi" ]; then
    pass "YKW4Recomp -r smoke.nro made $game/Smoke$exe"
else
    fail "YKW4Recomp -r smoke.nro exited $status, without $game/Smoke$exe and its .abi" "$out"
fi
if [ -f "$config/Smoke/Smoke.ini" ] && [ ! -e "$game/Smoke.ini" ]; then
    pass "its settings are $config/Smoke/Smoke.ini"
else
    fail "YKW4Recomp -r smoke.nro did not write its settings in $config/Smoke only" "$out"
fi



if [ -f "$game/Smoke$exe" ]; then
    run env -C "$game" "./Smoke$exe" -H
    if grep -q '^YKW4Recomp - Smoke' <<<"$out"; then
        pass "$game/Smoke$exe started the game (exit $status: $(tail -n 1 <<<"$out"))"
    else
        fail "$game/Smoke$exe exited $status before the game's banner" "$out"
    fi
    if grep -q 'YKW4Recomp - Smoke' "$state/Smoke/Smoke.log" 2>/dev/null && [ ! -e "$game/Smoke.log" ]; then
        pass "its log is $state/Smoke/Smoke.log"
    else
        fail "the game did not log in $state/Smoke only" "$out"
    fi
fi



nobinutils=
IFS=: read -ra folders <<<"$PATH"
for folder in "${folders[@]}"; do
    for tool in as ld as.exe ld.exe; do [ -e "$folder/$tool" ] && continue 2; done
    nobinutils+=${nobinutils:+:}$folder
done
run env PATH="$nobinutils" "$runtime" -r smoke.nro GccSmoke --backend=gcc --jobs 2
game=$data/games/GccSmoke
if [ "$status" -eq 0 ] && [ -f "$game/GccSmoke$exe" ] && [ -f "$game/GccSmoke.abi" ]; then
    pass "YKW4Recomp -r smoke.nro --backend=gcc made $game/GccSmoke$exe"
    run env -C "$game" "./GccSmoke$exe" -H
    if grep -q '^YKW4Recomp - GccSmoke' <<<"$out"; then
        pass "$game/GccSmoke$exe started the game (exit $status: $(tail -n 1 <<<"$out"))"
    else
        fail "$game/GccSmoke$exe exited $status before the game's banner" "$out"
    fi
else
    fail "YKW4Recomp -r smoke.nro --backend=gcc exited $status, without $game/GccSmoke$exe and its .abi" "$out"
fi



cp smoke.nro play.nro
run "$runtime" play.nro -H
if grep -q '^YKW4Recomp - play' <<<"$out"; then
    pass "YKW4Recomp play.nro recompiled and started the game (exit $status)"
else
    fail "YKW4Recomp play.nro exited $status before the game's banner" "$out"
fi


printf 'PFS0\0\0\0\0\0\0\0\0\0\0\0\0' >empty.nsp
run "$runtime" -x empty.nsp
if grep -q 'no base Program NCA' <<<"$out"; then
    pass "YKW4Recomp -x empty.nsp ran hactool"
else
    fail "YKW4Recomp -x empty.nsp exited $status without hactool unpacking it" "$out"
fi

if [ "$failed" -ne 0 ]; then
    echo "$runtime does not work as installed" >&2
    exit 1
fi
echo "$runtime works as installed"
