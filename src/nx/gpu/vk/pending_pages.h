/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_VK_PENDING_PAGES_H
#define SNX_VK_PENDING_PAGES_H
#include "common/cpu/aarch64/cpu.h"


typedef struct PendLeaf PendLeaf;
typedef struct {
    PendLeaf* leaf[1u << 16];
    u64 total;  
    u64 outside;    
} PendPages;

void pend_add(PendPages* t, u64 gva, u64 size, int delta);

int pend_hit(const PendPages* t, u64 gva, u64 size);

int pend_empty(const PendPages* t);

#endif
