/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_ASTC_H
#define SNX_ASTC_H
#include "common/cpu/aarch64/cpu.h"


void astc_decode_image(const u8* src, u32 w, u32 h, u32 bw, u32 bh, u8* dst, int srgb);

#endif
