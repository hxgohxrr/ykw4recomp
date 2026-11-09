#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

exe = Path(sys.argv[1]).resolve()
base = Path(sys.argv[2]).resolve()
base.mkdir(parents=True, exist_ok=True)

root = Path(tempfile.mkdtemp(prefix='zip-audit-', dir=base))
cases = [
    ('romfs/data/payload.txt', True), ('package/romfs/data/payload.txt', True),
    ('data/payload.txt', True), ('romfs/data/名前.txt', True),
    ('romfs/../escape.txt', False), ('../escape.txt', False),
    ('/romfs/data/a', False), ('C:/romfs/data/a', False),
    ('romfs/data/a:stream', False), ('romfs/data/NUL.txt', False),
    ('romfs/data/CONIN$', False), ('romfs/data/CONOUT$', False),
    ('romfs/data/COM¹.txt', False), ('romfs/data/LPT²', False),
    ('romfs/data/a.', False), ('romfs/data/a ', False),
]
for index, (name, valid) in enumerate(cases):
    archive, destination = root/f'{index}.zip', root/f'out-{index}'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(name, b'synthetic mod payload')
    subprocess.run([str(exe), str(archive), str(destination), str(int(valid))], check=True, timeout=10)
    if valid:
        files = [p for p in destination.rglob('*') if p.is_file()]
        assert len(files) == 1 and files[0].read_bytes() == b'synthetic mod payload'
    else:
        assert not destination.exists(), name
for label, names in [('duplicate', ['romfs/data/a', 'romfs/data/A']),
                      ('multiple', ['one/romfs/data/a', 'two/romfs/data/b'])]:
    archive, destination = root/f'{label}.zip', root/f'out-{label}'
    with zipfile.ZipFile(archive, 'w') as z:
        for name in names:
            z.writestr(name, b'synthetic')
    subprocess.run([str(exe), str(archive), str(destination), '0'], check=True, timeout=10)
    assert not destination.exists()
archive, destination = root/'symlink.zip', root/'out-symlink'
with zipfile.ZipFile(archive, 'w') as z:
    entry = zipfile.ZipInfo('romfs/data/link')
    entry.create_system = 3
    entry.external_attr = (0o120777 << 16)
    z.writestr(entry, '../../escape')
subprocess.run([str(exe), str(archive), str(destination), '0'], check=True, timeout=10)
assert not destination.exists() and not (root/'escape.txt').exists()
print('19 synthetic ZIP import cases passed; no real game or mod content used.')
