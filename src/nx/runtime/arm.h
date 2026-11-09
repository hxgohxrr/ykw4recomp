/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_NX_ARM_H
#define SNX_NX_ARM_H
#include "nx/loader/module_abi.h"

struct KThread;


extern const SnxArmModule* g_arm_mod;

extern const SnxArmRt g_arm_rt;

const SnxModule* arm_module(const SnxArmModule* module);

void arm_dispatch_init(void);

void arm_thread_run(struct KThread* t);

u32 arm_block_at(const void* host_pc, u64* offset);

#endif
