/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#include "nx/gpu/texture/depth.h"


u32 depth24_unpack(u32 tic_format, u32 texel, u8* stencil) {
    if (tic_format == 0x0D || tic_format == 0x2A || tic_format == 0x2B) {
        *stencil = tic_format == 0x2A ? 0 : (u8)(texel >> 24);
        return texel & 0xFFFFFF;
    }
    *stencil = (u8)texel;
    return texel >> 8;
}
