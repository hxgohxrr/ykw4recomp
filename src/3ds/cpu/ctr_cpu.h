/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_CPU_H
#define SNX_CTR_CPU_H
#include "common/cpu/arm/checked.h"
#include "3ds/memory/ctr_memory.h"


typedef struct {
    SnxArmCheckedCPU state;
    SnxCtrAddressSpace* space;
    uint64_t reservation_epoch;
    uint32_t reservation_address, reservation_size;
} SnxCtrCpu;

bool snx_ctr_cpu_init(SnxCtrCpu* cpu, SnxCtrAddressSpace* space, uint32_t pc, uint32_t sp, bool thumb);
#endif
