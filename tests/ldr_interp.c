/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/cpu/aarch64/cpu.h"
void snx_interp(CPU* c, u64 entry);
u64 snx_step(CPU* c, u32 word, u64 pc);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "ldr_interp.inc"
#pragma GCC diagnostic pop
GuestFn snx_interp_entry = snx_interp;
