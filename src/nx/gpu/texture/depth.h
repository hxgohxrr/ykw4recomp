/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_TEXTURE_DEPTH_H
#define SNX_TEXTURE_DEPTH_H
#include "common/cpu/aarch64/cpu.h"


u32 depth24_unpack(u32 tic_format, u32 texel, u8* stencil);

#endif
