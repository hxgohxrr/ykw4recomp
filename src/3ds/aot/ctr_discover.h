/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_DISCOVER_H
#define SNX_CTR_DISCOVER_H
#include "3ds/loader/ctr_image.h"
#define SNX_CTR_AOT_BLOCK_WORDS 32u
#define SNX_CTR_PLAN_VISITED 1u
#define SNX_CTR_PLAN_ENTRY 2u
#define SNX_CTR_PLAN_STOP 4u

typedef struct {
    uint32_t address, bytes, entries, instructions;
    unsigned char *arm, *thumb;
} SnxCtrPlan;
bool snx_ctr_discover(const SnxCtrImage* image, SnxCtrPlan* plan, char* error, size_t error_size);
void snx_ctr_plan_free(SnxCtrPlan* plan);
unsigned snx_ctr_plan_length(const SnxCtrPlan* plan, uint32_t pc, bool thumb);
#endif
