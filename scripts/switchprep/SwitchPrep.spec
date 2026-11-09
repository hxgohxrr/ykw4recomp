# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
here = Path(SPECPATH)
root = here.parents[1]
a = Analysis([str(here / 'switchprep.py')], pathex=[str(here)], binaries=[], datas=[],
             hiddenimports=[], hookspath=[], hooksconfig={}, runtime_hooks=[], excludes=[],
             noarchive=False, optimize=0)

a.binaries = [entry for entry in a.binaries
              if not entry[0].lower().startswith('api-ms-win-') and entry[0].lower() != 'ucrtbase.dll']
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, a.binaries, a.datas, [], name='SwitchPrep', debug=False,
          bootloader_ignore_signals=False, strip=False, upx=False, console=True,
          disable_windowed_traceback=False, argv_emulation=False, target_arch=None,
          codesign_identity=None, entitlements_file=None,
          icon=str(root / 'dist/icons/launcher-normal.ico'))
