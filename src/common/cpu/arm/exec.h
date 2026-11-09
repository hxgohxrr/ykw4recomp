/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_ARM_EXEC_H
#define SNX_ARM_EXEC_H
#include "common/cpu/arm/arm.h"
#include <stddef.h>


typedef struct SnxArmExecRange {
    uint64_t address;
    const unsigned char* data;
    size_t size;
} SnxArmExecRange;


typedef int (*SnxArmSyscallFn)(void* context, uint32_t imm, uint64_t number, const uint64_t arguments[6]);

typedef struct SnxArmExec SnxArmExec;


void snx_arm_exec_symbols(void);

SnxArmExec* snx_arm_exec_new(char* error, size_t error_size);
void snx_arm_exec_free(SnxArmExec* exec);

SnxArmBlock snx_arm_exec_block(SnxArmExec* exec, const SnxArmExecRange* ranges, size_t range_count, uint32_t pc,
                               int thumb, const unsigned char* base);

int snx_arm_exec_run(SnxArmExec* exec, SnxArmCPU* cpu, const SnxArmExecRange* ranges, size_t range_count,
                     SnxArmSyscallFn syscall, void* context);

#endif
