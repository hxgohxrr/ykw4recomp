/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_ENGINES_INT_H
#define SNX_ENGINES_INT_H
#include <string.h>
#include "nx/runtime/main.h"
#include "nx/runtime/options.h"
#include "nx/gpu/renderer.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include <stdio.h>
#include <stdlib.h>
#include "nx/gpu/gpu.h"
#include "nx/runtime/clock.h"


void query_get(GpuChannel* ch, u32* r, int base, u32 v);


u8* bl_span_ptr(u64 base, u32 wbytes, u32 h, u32 bh, u32 z, u32 bd, u64* span);



void mme_run(GpuChannel* ch, u32 idx, u32 np);

#endif
