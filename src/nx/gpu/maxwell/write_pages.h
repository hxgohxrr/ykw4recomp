/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GPU_WRITE_PAGES_H
#define SNX_GPU_WRITE_PAGES_H
#include "common/cpu/aarch64/cpu.h"


typedef struct WritePageLeaf WritePageLeaf;
typedef struct {
    WritePageLeaf* leaf[1u << 16];
    u64 far;  
} WritePages;
void write_pages_stamp(WritePages* t, u64 a, u64 size, u64 gen);

int write_pages_since(const WritePages* t, u64 since, u64 a, u64 size);

#endif
