/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GNU_JIT_H
#define SNX_GNU_JIT_H
#include "common/cpu/aarch64/cpu.h"
#include "common/cpu/execution/execution.h"
#include <stddef.h>

enum { GNU_JIT, GNU_AOT };
struct snx_recompiler;
struct snx_recompiled;
typedef struct GnuJit {
    int mode;
    const char* cc;  
    struct snx_recompiler* recompiler;
    GuestFn* table;  
    struct snx_recompiled** modules;
    size_t module_count, module_capacity;
    SnxExecution aot;
    int verbose;
    size_t step;  
    uint64_t changed_lo, changed_hi;  
} GnuJit;

int gnu_jit_init(GnuJit* jit, int mode, const char* cc, const char* cache, char* error, size_t error_size);

void gnu_jit_reset(GnuJit* jit);
void gnu_jit_destroy(GnuJit* jit);

int gnu_jit_prepare(GnuJit* jit, const SnxExecRange* ranges, size_t count, uint64_t entry, char* error,
                    size_t error_size);
void gnu_jit_bind(GnuJit* jit, CPU* cpu);

void gnu_jit_call(GnuJit* jit, CPU* cpu, uint64_t pc);

void gnu_jit_install(GnuJit* jit, uint64_t pc, GuestFn function);

void gnu_jit_invalidate(GnuJit* jit, uint64_t address, uint64_t length);

int gnu_jit_changed(GnuJit* jit, uint64_t pc);

int gnu_jit_commit(GnuJit* jit, uintptr_t address);
#endif
