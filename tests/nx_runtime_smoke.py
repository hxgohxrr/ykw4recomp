# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors
"""Recompile synthetic homebrew and run its Horizon SVCs with headless Vulkan."""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def make_nro():
    image = bytearray(0x200)

    def word(offset, value):
        struct.pack_into('<I', image, offset, value)

    word(0, 0x14000020)   
    image[0x10:0x14] = b'NRO0'
    word(0x18, len(image))
    for offset, value in ((0x20, 0), (0x24, 0x100), (0x28, 0x100), (0x2C, 0x80), (0x30, 0x180), (0x34, 0x80)):
        word(offset, value)
    marker = b'NX runtime smoke\n'
    image[0x100 : 0x100 + len(marker)] = marker
    word(0x80, 0x10000400)   
    word(0x84, 0xD2800001 | len(marker) << 5)   
    word(0x88, 0xD40004E1)   
    word(0x8C, 0xD40000E1)   
    word(0x90, 0xD4200000)   
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--relocate', action='store_true')
    parser.add_argument('--frame-profile', action='store_true')
    parser.add_argument('--artifact-dir', type=Path, default=Path('build/nx-smoke-failures'))
    args = parser.parse_args()
    runtime, repo, artifacts = args.runtime.resolve(), args.repo.resolve(), args.artifact_dir.resolve()
    with tempfile.TemporaryDirectory(prefix='YKW4Recomp-nx-smoke-') as temporary:
        scratch = Path(temporary)
        fixture, output = scratch / 'homebrew with spaces.nro', scratch / 'output with spaces'
        fixture.write_bytes(make_nro())
        env = os.environ.copy()
        env.pop('SNX_GAME', None)
        env.pop('SNX_FRAME_PROFILE', None)
        for name in ('APPDATA', 'LOCALAPPDATA', 'USERPROFILE', 'HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_STATE_HOME', 'XDG_CACHE_HOME'):
            env[name] = str(scratch / name.lower())
        if args.frame_profile:
            env['SNX_FRAME_PROFILE'] = '1'
        try:
            compile_command = [
                str(runtime),
                '--repo',
                str(repo),
                '-r',
                str(fixture),
                'NX Smoke',
                '--runtime',
                str(runtime),
                '--jobs',
                '2',
                '--outdir',
                str(output),
            ]
            compiled = subprocess.run(
                compile_command, cwd=scratch, env=env, capture_output=True, timeout=args.timeout, check=False
            )
            (scratch / 'compile.stdout').write_bytes(compiled.stdout)
            (scratch / 'compile.stderr').write_bytes(compiled.stderr)
            if compiled.returncode:
                raise RuntimeError(f'NRO recompilation returned {compiled.returncode}')
            folder = output / 'NX Smoke'   
            if args.relocate:
                destination = scratch / 'relocated installation' / 'NX Smoke'
                shutil.copytree(folder, destination)
                folder = destination
            launcher = folder / ('NX Smoke.exe' if os.name == 'nt' else 'NX Smoke')
            result = subprocess.run(
                [str(launcher), '-H'],
                cwd=folder,
                env=env,
                capture_output=True,
                timeout=args.timeout,
                check=False,
            )
            (scratch / 'runtime.stdout').write_bytes(result.stdout)
            (scratch / 'runtime.stderr').write_bytes(result.stderr)
            log = folder / 'NX Smoke.log'
            text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
            if result.returncode or '[game] NX runtime smoke' not in text or 'The game exited (code 0)' not in text:
                raise RuntimeError(
                    f'NX runtime returned {result.returncode} or did not execute the Horizon SVC markers'
                )
            if args.frame_profile:
                match = re.search(r'perf: enabled; csv=(.+?) file_open=1;', text)
                if not match or not Path(match[1]).is_file():
                    raise RuntimeError('Frame profile CSV was not created')
                rows = Path(match[1]).read_text(encoding='utf-8').splitlines()
                if len(rows) < 2 or 'max_since_start_ns' not in rows[0] or 'perf: ' not in text:
                    raise RuntimeError('Frame profile did not flush a final report')
            print('Synthetic NRO recompiled and ran Horizon OutputDebugString/ExitProcess with headless Vulkan.')
            return 0
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            artifacts.mkdir(parents=True, exist_ok=True)
            saved = artifacts / f'nx-smoke-{time.time_ns()}'
            shutil.copytree(scratch, saved)
            print(f'NX runtime smoke: {error}; saved {saved}', file=sys.stderr)
            return 1


if __name__ == '__main__':
    raise SystemExit(main())
