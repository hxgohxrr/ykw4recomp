<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright 2026 the ScarletNX contributors -->
# Licenses and credits

**Project logo by Pabl06**, used in the launcher header and project website.
See [artwork notes](../dist/icons/LAUNCHER-ARTWORK.md).

YKW4Recomp is based on [ScarletNX](https://github.com/scarletnx/ScarletNX)
and its contributors, in turn derived from **SwitchRecomp by MZ Labs**,
copyright 2026 MZ Labs. See [source provenance](SOURCE-PROVENANCE.json)
and [local changes](LOCAL-CHANGES.md).
Fork: [GPL-3.0-or-later](../LICENSE). MZ Labs code retains its
[Apache-2.0](../LICENSE.apache) headers and [NOTICE](../NOTICE).
Generated modules include runtime ABI code. Game assets, keys and firmware are excluded.

| Component | License |
| --- | --- |
| FFmpeg | LGPL-2.1-or-later |
| hactool / cJSON / ConvertUTF | ISC / MIT / Unicode license |
| LibHac storage code, ported into hactool by `cmake/hactool-libhac.patch` | MIT |
| OpenSSL | Apache 2.0 |
| LZ4 (the library, `lib/`) | BSD-2-Clause |
| Vulkan-Headers | Apache 2.0 |
| volk / Vulkan Memory Allocator | MIT |
| SDL3 | zlib |
| Launcher cJSON | MIT, `src/nx/runtime/vendor/cjson/LICENSE` |
| Launcher miniz | MIT, `src/nx/runtime/vendor/miniz/LICENSE` |
| lunasvg / plutovg (the splash's SVGs) | MIT / MIT, with FreeType's rasterizer and stroker (FreeType License) and stb (MIT or public domain) |
| GUI VGA BIOS font (SeaBIOS `vgafont16`, originally Joseph Gil's font collection) | Public domain |

The GUI font's original notice and pinned source are preserved in
[`src/gui/FONT-NOTICE.txt`](../src/gui/FONT-NOTICE.txt).

Code ported into the repository, and assets taken from other projects. In these places, the files whose header names
a source keep that source's copyright lines and license, most with the revision they came from; the drawables of
`dist/controls/` have theirs in [its README](../dist/controls/README.md):

| Where | From | License |
| --- | --- | --- |
| `cmake/hactool-libhac.patch` (the storages it adds to hactool) | LibHac, as Ryubing maintains it | MIT |
| `src/3ds/gpu/`, `src/3ds/audio/`, `src/3ds/services/dsp.c` | Azahar (Citra) | GPL-2.0-or-later |
| `src/3ds/gpu/shader.c` and `shader.h` (the instruction fields) | nihstro | BSD-3-Clause |
| `src/common/network/`, `src/nx/hle/services/network/` | Eden and yuzu | GPL-3.0-or-later, GPL-2.0-or-later |
| `src/nx/gpu/video/vp9_rebuild.cpp` | Eden and yuzu | GPL-3.0-or-later, GPL-2.0-or-later |
| `tests/ipc_message.c` (the HIPC, CMIF and TIPC request builders) | libnx | ISC |
| `dist/controls/*.xml`, and `controls.png` made from them | yuzu's Android overlay, as Eden and rEden carry it | GPL-3.0-or-later |

Installed license texts: `share/licenses/scarletnx/`. AppImage system-library licenses
and package manifest: `share/licenses/`. Component license files contain the full terms.

Community credits: [README](../README.md#special-thanks).

Wimsy ([SVG metadata](../dist/wimsy.svg)): drawn by Liz, GPL-3.0-or-later.
The splash's Fuddles ([SVG metadata](../dist/splash/wimsy-fuddles.svg)): Wimsy with Fuddles' tails, circuits and
colours, by AO55 after Liz's Wimsy, GPL-3.0-or-later.

Launcher normal/DLC artwork supplied by hxgohxrr, described as redraws. See [artwork notes](../dist/icons/LAUNCHER-ARTWORK.md).

## Optional Spanish translation

The launcher links to [REY3DS / YKW4_ES](https://github.com/REY3DS/YKW4_ES),
version 7.0, for the Spanish fan translation. The patch is obtained separately
from its authors and is not included in this launcher package.
Their README credits Vitaliy K. (@Vitalkrilov) for the English text and the
Yo-kai Watch 4 English Translation Mod Team for the English base.
The authors require game update 2.2.0. See their release and license for the
patch's own terms. These translation credits do not change the launcher's license.

## Package preparation assistant

SwitchPrep Python helper: GPL-3.0-or-later, `scripts/switchprep/LICENSE`. hactool by SciresM: ISC, with ScarletNX GPL-3.0-or-later adaptations; the ScarletNX LibHac storage port and its original MIT notices are retained. Backend links OpenSSL (Apache 2.0) and LZ4 (BSD). The frozen helper uses Python (PSF) and the PyInstaller bootloader exception, with notices included in the binary package.
