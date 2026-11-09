#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import hashlib
import json
import os
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SKIP = {'.git', '.cache', 'deps', '__pycache__', 'build', 'stage'}
BAD_SUFFIXES = {'.exe', '.dll', '.obj', '.o', '.so', '.a', '.pdb', '.dmp', '.keys',
                '.nsp', '.xci', '.nca', '.nso', '.nro', '.npdm', '.nsz', '.xcz',
                '.tik', '.cert', '.romfs', '.jitcache', '.snxs', '.spv', '.ppm',
                '.zip', '.rar', '.7z', '.zst', '.bin', '.log', '.ini'}
BAD_PARTS = {'roms', 'romfs', 'exefs', 'dlcexefs', 'setup-work', 'shader_cache',
             'save', 'saves', 'captures', 'private'}
SOURCE_CONFIGS = {'src/gui/themes/default/theme.ini', 'src/gui/themes/yorha/theme.ini'}
PATTERNS = {
    'GitHub credential': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b'),
    'Slack credential': re.compile(rb'\bxox[baprs]-[A-Za-z0-9-]{20,}\b'),
    'AWS access key': re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),
    'Discord bot credential': re.compile(rb'\b[A-Za-z0-9_-]{24,28}\.[A-Za-z0-9_-]{6}\.[A-Za-z0-9_-]{27,}\b'),
    'Console key material': re.compile(rb'(?im)^\s*(?:master_key_\w+|titlekek_\w+|header_key|key_area_key_\w+)\s*=\s*[0-9a-f]{32,64}\s*$'),
    'Private development path': re.compile(rb'(?i)(?:[CD]:[\\/]Users[\\/](?!Public[\\/])[^\\/\s"<>]+[\\/]|D:[\\/]ykw4decomp[\\/])'),
}


def inspect(root=ROOT):
    failures, manifest = [], []
    paths = []
    for folder, directories, files in os.walk(root, followlinks=False):
        directories[:] = [name for name in directories if name not in SKIP and not name.startswith('build-')]
        paths.extend(Path(folder)/name for name in files)
        paths.extend(Path(folder)/name for name in directories if (Path(folder)/name).is_symlink())
    for path in sorted(paths):
        rel = path.relative_to(root)
        if any(part in SKIP or part.startswith('build-') for part in rel.parts):
            continue
        if path.is_symlink():
            failures.append((rel.as_posix(), 'symlink'))
            continue
        if not path.is_file():
            continue
        name = rel.as_posix()
        if any(part.lower() in BAD_PARTS or part.lower().endswith('_data') for part in rel.parts):
            failures.append((name, 'game/private directory'))
        if (path.suffix.lower() in BAD_SUFFIXES and name not in SOURCE_CONFIGS) or '_code.' in path.name.lower():
            failures.append((name, 'binary/game/cache/archive file'))
        if path.name.lower() in {'ykw4.ico', 'ykw4-normal.ico', 'ykw4.png', '.env', 'extract.c'}:
            failures.append((name, 'excluded resource or private file'))
        if path.stat().st_size > 2*1024*1024:
            failures.append((name, 'unexpected file larger than 2 MiB'))
            continue
        data = path.read_bytes()
        if data.startswith((b'NSO0', b'NRO0', b'PFS0', b'HEAD', b'MZ', b'\x7fELF')):
            failures.append((name, 'executable/container magic'))
        for label, pattern in PATTERNS.items():
            if pattern.search(data):
                failures.append((name, label))
        manifest.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    for name in ('LICENSE', 'LICENSE.apache', 'NOTICE', 'README.md', 'BUILDING.md',
                 'src/nx/runtime/vendor/miniz/LICENSE', 'src/nx/runtime/vendor/cjson/LICENSE',
                 'docs/CREDITS.md', 'dist/controls/README.md', 'dist/wimsy.svg'):
        if not (root/name).is_file():
            failures.append((name, 'required license/document missing'))
    notice = (root/'NOTICE').read_text() if (root/'NOTICE').exists() else ''
    if 'Copyright 2026 MZ Labs' not in notice:
        failures.append(('NOTICE', 'upstream copyright missing'))
    return failures, manifest


if __name__ == '__main__':
    failures, manifest = inspect()
    if failures:
        for path, reason in failures:
            print(f'FAIL {path}: {reason}')
        sys.exit(1)
    print(json.dumps({'result': 'passed', 'files': len(manifest),
                      'bytes': sum(row['bytes'] for row in manifest),
                      'scope': 'Allowlisted source content, selected credential patterns and required notices. Not a legal certification or exhaustive secret scanner.'}, indent=2))
