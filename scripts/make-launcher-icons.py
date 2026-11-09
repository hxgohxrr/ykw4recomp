#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path
from PIL import Image

base = Path(__file__).resolve().parents[1]/'dist/icons'
for name in ('launcher-normal', 'launcher-dlc'):
    with Image.open(base/(name+'.png')) as image:
        image.convert('RGBA').save(base/(name+'.ico'), format='ICO',
                                  sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
