/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_ARM_CHECKED_H
#define SNX_ARM_CHECKED_H
#include "common/cpu/arm/arm.h"


#define SNX_ARM_CHECKED_ABI      1u
#define SNX_ARM_ACCESS_READ      1u
#define SNX_ARM_ACCESS_WRITE     2u
#define SNX_ARM_ACCESS_FETCH     3u

#define SNX_ARM_ACCESS_MULTI     0x100u

#define SNX_ARM_ACCESS_EXCLUSIVE 0x200u
#define SNX_ARM_EXCLUSIVE_FAILED 2u


typedef uint32_t (*SnxArmAccess)(void* context, uint32_t operation, uint32_t address, uint32_t size, void* bytes);
typedef struct {
    SnxArmCPU cpu;
    uint32_t abi;
    void* context;
    SnxArmAccess access;
} SnxArmCheckedCPU;
_Static_assert(offsetof(SnxArmCheckedCPU, cpu) == 0, "checked CPU prefix");
#endif
