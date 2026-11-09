<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright 2026 the ScarletNX contributors -->
# Development

[Architecture](ARCHITECTURE.md) · [build/toolchains](BUILD.md) · [usage](USAGE.md) · [source provenance](SOURCE-PROVENANCE.json) · [contributing](../CONTRIBUTING.md).
Run from checkout. POSIX examples: Windows uses `python`, `.exe`, PowerShell env syntax.
The build tree's drivers work in the user's data folder, as an installed copy does ([Folders](USAGE.md#folders));
the examples name its folders on Linux, and `--repo DIR` keeps a run's files in DIR instead:

```sh
games=~/.local/share/YKW4Recomp/games work=~/.cache/YKW4Recomp/work
```

## Rebuild / checks

```sh
cmake --build build
./build/YKW4Recomp -r "$work/Game" Game --jobs 8
ctest --test-dir build --output-on-failure
scripts/format.sh --check
python3 tests/run_filesystem_tree.py --cc clang --sanitize
clang -fsyntax-only -Wall -fno-strict-aliasing -fwrapv -I src src/common/cpu/aarch64/cpu_helpers.c
```

Runtime relink deploys to the data folder's `games/` by default (`SNX_GAMES_DIR`). Recompiler/`src/common/cpu/aarch64/` edits require `-r`.
Avoid builds during timing runs. Formatting: clang-format 23 + `uv`/pinned Ruff; omit `--check` to write.
FS tests use production tree/HLE + synthetic temp data; `--only tree|service` selects a suite.

The source audit runs on pushes and pull requests
(`.github/workflows/source-audit.yml`). Windows builds are started manually with
`.github/workflows/windows.yml`; experimental Linux, macOS, iOS and Android builds
use `.github/workflows/platforms.yml`. The platform workflow can attach packages
to an existing draft release when a release ID is supplied. This checkout does
not configure automatic nightly publishing or Discord build announcements.
Formatting is checked locally (`scripts/format.sh --check`).
Verify affected scenes manually, repeat intermittent failures, record evidence/scope.
Test guest semantics + malformed IPC with synthetic cases.

## Run / diagnose

```sh
(cd "$games/<id>" && ./<id> --seconds=60 --script=15000:400,16000:1 --press_len=300 --shot=out)
python3 tools/run_game.py <id> --runs 5 --seconds 60 --dump "$work/Game" --script ""
python3 tools/symbols.py "$work/Game" cf34380
python3 tools/disasm.py "$work/Game" NvRmStreamFlushSync
python3 tools/disasm.py "$work/Game" cf34240 cf343c0
```

- Runtime: `--seconds=N` / `--frames=N`; `--shot=out` → CWD `out.ppm`; `--shot_every=N` → every N frames.
  `--script=ms:hexbuttons,...`, `--press_len=ms` (default 300).
  Masks: A/B/X/Y `1/2/4/8`, L/R `40/80`, ZL/ZR `100/200`, Plus/Minus `400/800`,
  left/up/right/down `1000/2000/4000/8000`.
- `run_game.py`: Catherine New Game sequence by default; **`--script ""` disables it**.
  `--out DIR` → `runN.log` + PNG (PPM without Pillow); default fresh temp dir.
  Summary: exit, PC/symbol (`--dump`), frames/FPS. Unknown args pass through, e.g. `--fps_limit=0`.
- Guest VAs: hex; disasm function queries: substrings. Disasm requires `llvm-objcopy` + `llvm-objdump`.
  `--log=2`: verbose; Windows `--profile=1`: `profile.txt` every 10 s.

## Shaders

```sh
SNX_SHADER_CORPUS=corpus "$games/Game/Game"
build/snx-shaderc corpus --validate --out translated
```

`--validate` needs `spirv-val`. `--out` writes SPIR-V modules for build comparisons.
PowerShell: `$env:SNX_SHADER_CORPUS='corpus'`. Where the game gets no environment (Android), its INI's `[Debug]
diag=1` keeps each shader's entry in `captures/shaders` beside the game, for the same `snx-shaderc`.

## Upstream / provenance

```sh
scripts/import-upstream.sh SwitchRecomp_v143.zip v143
```

`upstream`: MZ Labs releases + formatted copies; `upstream-vN`: original imports; fork base: `upstream-v142`.
Port to `main` manually (different layouts). Exclude dumps/keys/generated modules/dependency builds from Git.
Preserve headers + [credits/install notices](CREDITS.md).
