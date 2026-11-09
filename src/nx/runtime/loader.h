/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_LOADER_H
#define SNX_LOADER_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/loader/module_abi.h"

extern const SnxModule* g_mod;
extern GuestFn* g_disp;
int loader_load(void);
extern u64 g_code_lo, g_code_hi;

#endif
