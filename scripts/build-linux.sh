#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$HOME/.cache/ykw4recomp/build-linux"}
stage_dir=${2:-"$HOME/.local/share/ykw4recomp-linux"}
jobs=${YKW4_BUILD_JOBS:-4}
command -v clang >/dev/null
command -v cmake >/dev/null
command -v ninja >/dev/null
command -v llvm-config >/dev/null
cmake --preset launcher-linux -S "$source_dir" -B "$build_dir" -DSNX_LLVM_CONFIG="$(command -v llvm-config)"
cmake --build "$build_dir" --target YKW4Recomp --parallel "$jobs"
cmake --install "$build_dir" --prefix "$stage_dir"
if [ -n "${YKW4_HACTOOL_SOURCE:-}" ]; then
    : "${YKW4_LZ4_SOURCE:?Set YKW4_LZ4_SOURCE to the LZ4 source directory}"
    cmake -S "$source_dir/scripts/switchprep/backend" -B "$build_dir/switchprep-backend" -G Ninja \
        -DHACTOOL_SOURCE="$YKW4_HACTOOL_SOURCE" -DLZ4_SOURCE="$YKW4_LZ4_SOURCE" \
        -DOPENSSL_ROOT="${YKW4_OPENSSL_ROOT:-$build_dir/openssl}"
    cmake --build "$build_dir/switchprep-backend" --parallel "$jobs"
    cmake --install "$build_dir/switchprep-backend" --prefix "$stage_dir"
fi
printf 'Linux launcher: %s/bin/YKW4Recomp\n' "$stage_dir"
