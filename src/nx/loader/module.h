/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors
   Derived from SwitchRecomp module.py, Copyright 2026 MZ Labs, Apache-2.0.
   Retain LICENSE.apache and NOTICE; the fork's changes are GPL-3.0-or-later. */
#ifndef SNX_TOOL_MODULE_H
#define SNX_TOOL_MODULE_H

#include "nx/loader/loader.h"
#include "common/cpu/recompiler/codegen.h"

typedef struct {
    const char* title;
    size_t chunk_instrs;
    int interp_only, quiet;
    const SnxU64* hints;
    size_t hint_count;
    unsigned abi;
} SnxCodegenOptions;


#ifdef __ANDROID__
#define SNX_CHUNK_INSTRUCTIONS 20000
#else
#define SNX_CHUNK_INSTRUCTIONS 90000
#endif


typedef struct {
    SnxU64 address;
    size_t function;
} SnxModuleEntry;
SnxModuleEntry* snx_module_entries(const SnxCodegen* codegen, size_t* count);

char* snx_chunk_label(const SnxCodegen* codegen, size_t first, size_t last);

void snx_report_unit(size_t done, size_t count, const char* name, const char* label, int cached, double seconds);

#endif
