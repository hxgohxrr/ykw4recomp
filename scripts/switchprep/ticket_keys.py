# SPDX-License-Identifier: GPL-3.0-or-later

# copyright 2026 the ScarletNX contributors. Notices retained under GPL-3.0-or-later.
import struct


def common_ticket_key(data):

    if len(data) < 4:
        return None
    signature = struct.unpack_from('<I', data)[0]
    offsets = {0x10000: 0x240, 0x10003: 0x240, 0x10001: 0x140,
               0x10004: 0x140, 0x10002: 0x80, 0x10005: 0x80, 0x10006: 0x40}
    offset = offsets.get(signature)
    if offset is None or len(data) < offset + 0x170 or data[offset + 0x141]:
        return None
    return data[offset + 0x160:offset + 0x170].hex(), data[offset + 0x40:offset + 0x50].hex()
