/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_PROCESS_H
#define SNX_CTR_PROCESS_H
#include "3ds/cpu/ctr_cpu.h"
#include "3ds/loader/ctr_image.h"
#define SNX_CTR_STACK_TOP   0x10000000u
#define SNX_CTR_INITIAL_TLS 0x1ff82000u

typedef struct SnxCtrProcess SnxCtrProcess;

SnxCtrProcess* snx_ctr_process_new(const SnxCtrImage* image, SnxCtrModel model);

bool snx_ctr_process_mount_fs(SnxCtrProcess* process, const char* directory);
void snx_ctr_process_free(SnxCtrProcess* process);
SnxArmCPU* snx_ctr_process_cpu(SnxCtrProcess* process);
SnxCtrAddressSpace* snx_ctr_process_space(SnxCtrProcess* process);
#endif
