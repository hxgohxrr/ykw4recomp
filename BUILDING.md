# Building the desktop launcher

Build from this source tree, not from a prepared game directory. Builds require
network access to download the pinned dependency sources unless already cached.
No game files or console keys are required to build or run the synthetic tests.

## Windows with MSYS2 CLANG64

Use a consistent Clang/LLVM toolchain; do not mix incompatible C++ runtimes.
Install MSYS2 and open its CLANG64 shell. Install the following packages:

```sh
pacman -S --needed git make patch perl mingw-w64-clang-x86_64-clang \
  mingw-w64-clang-x86_64-llvm mingw-w64-clang-x86_64-lld \
  mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja \
  mingw-w64-clang-x86_64-nasm mingw-w64-clang-x86_64-python
```

Configure and build from the repository root:

```sh
cmake --preset launcher-windows \
  -DSNX_LLVM_CONFIG="$(cygpath -m "$(command -v llvm-config)")" \
  -DSNX_FFMPEG_SH="$(cygpath -m /usr/bin/sh.exe)" \
  -DSNX_FFMPEG_MAKE="$(cygpath -m /usr/bin/make.exe)" \
  -DPERL_EXECUTABLE="$(cygpath -m /usr/bin/perl.exe)" \
  -DSNX_OPENSSL_MAKE="$(cygpath -m /usr/bin/make.exe)"
cmake --build --preset launcher-windows --parallel 4
```

The executable is `build/launcher-windows/YKW4Recomp.exe`. The game-specific
preparation flow is selected by naming the deployed executable `YKW4.exe`.
Copy launcher language JSON files into a sibling `langs` directory for a portable
deployment. The full runtime's LLVM/DLL dependency closure must accompany it.

`scripts/bootstrap.ps1` is an alternative toolchain bootstrap. Its Clang package
alone does not supply LLVM development libraries; supply a matching
`SNX_LLVM_CONFIG`. OpenSSL also requires working Perl modules and GNU make.

## Linux and WSL

The `launcher-linux` preset enables the native Python/Tk desktop launcher and
disables the unrelated GNUABI and 3DS runtimes. The bare runtime opens the
launcher; explicit command-line arguments retain developer/runtime flows.
The Linux interface covers settings, preparation, mods, Discord, mobile builds
and local binding without Wine or Win32 controls. It requires Python 3.10+
and Tk at runtime (`python3-tk` on Ubuntu).
Build without game data or console keys. On Ubuntu 24.04, install Clang/LLVM
development files, CMake, Ninja, make, patch, Perl, NASM, pkg-config, Python/Tk
and SDL3 desktop build prerequisites. Pinned dependencies are fetched from
source where needed. See `docs/BUILD.md` for the complete dependency list.

```sh
sh scripts/build-linux.sh
```

The build defaults to `$HOME/.cache/ykw4recomp/build-linux` and the installation
to `$HOME/.local/share/ykw4recomp-linux`. The optional first and second arguments
select those directories. Prefer the Linux filesystem in WSL, not `/mnt/c` or
`/mnt/d`, for build output. Run the printed `bin/YKW4Recomp` in an X11/Wayland
desktop (including WSLg). A headless test or build is not a gameplay/GPU test.
See `docs/MOBILE-BUILDS.md` for mobile tools and explicit consent. IPA builds
require a Mac with Xcode, not WSL.

Package preparation additionally needs a Linux hactool backend and your own
keys. Extracted ExeFS/RomFS preparation does not need that backend. To build
and install it alongside the launcher, supply the matching patched hactool and
LZ4 sources (not automatically supplied by the public launcher checkout):

```sh
YKW4_HACTOOL_SOURCE=/path/to/patched-hactool \
YKW4_LZ4_SOURCE=/path/to/lz4 \
sh scripts/build-linux.sh
```

`YKW4_OPENSSL_ROOT` can select an existing static OpenSSL prefix. See
`docs/LINUX-LAUNCHER.md` for installation selection and desktop limitations,
and `docs/local-module-binding.md` before preparing or migrating a module.

## Tests and source audit

```sh
python scripts/audit_public_source.py
ctest --test-dir build/launcher-windows \
  -R '^(launcher_update|discord_presence|window_icon|launcher_name|launcher_zip|launcher_icon)$' \
  --output-on-failure
```

These tests cover selected launcher components, not a complete first setup or
gameplay. Use `docs/BETA-TESTING.md` for the outstanding interactive checks.
Keep the matching LLVM/toolchain DLL directory on PATH when running tests.

## Redistribution

Source-only publication contains no binary libraries. CMake fetches pinned
dependency archives and checks their hashes. Before publishing a binary ZIP,
include its exact corresponding modified source and applicable dependency
sources, recipes, licenses and notices. Do not publish a folder after importing
game content. Source audit success does not establish complete legal compliance.


## Optional Windows package assistant

The runtime's extracted-only option stays enabled. SwitchPrep is built separately
from `scripts/switchprep/switchprep.py` with Python 3.12+ and PyInstaller 6.16.0;
see that directory's README. Place `SwitchPrep.exe` beside `YKW4.exe` and the
hactool backend in `backend/hactool.exe`. The standalone backend CMake recipe
accepts the patched hactool source, OpenSSL static prefix, and LZ4 source prefix.
The binary beta's corresponding-source archive includes those exact sources.
Retain the backend/runtime licenses when packaging. No console keys are needed
to build either executable or run the synthetic tests.

Windows NSP preparation can be embedded with
`-DSNX_SWITCHPREP_EXECUTABLE=/absolute/path/to/SwitchPrep.exe` after building
`scripts/switchprep/SwitchPrep.spec` with PyInstaller. The helper is embedded as
resource 5001; ship `backend/hactool.exe` and the dependency licenses/sources.
The launcher extracts the helper only into its unique preparation attempt and
removes that temporary executable after it exits. Extracted-only builds use the
selected data directory for driver paths, independently of the developer build drive.
