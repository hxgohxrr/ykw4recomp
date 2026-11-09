/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_RECOMPILER_LIBRARY_H
#define SNX_RECOMPILER_LIBRARY_H

#include <stddef.h>
#include <stdint.h>
#include "common/cpu/aarch64/cpu.h"
#include "common/cpu/arm/arm.h"


#if defined(SNX_RECOMPILER_EXPORTS) && defined(_WIN32)
#define SNX_RECOMPILER_API __declspec(dllexport)
#elif defined(SNX_RECOMPILER_EXPORTS)
#define SNX_RECOMPILER_API __attribute__((visibility("default")))
#else
#define SNX_RECOMPILER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif




#define SNX_RECOMPILER_FLAG_LLVM   1  
#define SNX_RECOMPILER_FLAG_GCCJIT 2  

struct snx_recompiler;


SNX_RECOMPILER_API struct snx_recompiler* snx_recompiler_new(int flags);
SNX_RECOMPILER_API void snx_recompiler_free(struct snx_recompiler* r);

SNX_RECOMPILER_API int snx_recompiler_has(int flags);

SNX_RECOMPILER_API unsigned snx_recompiler_abi(void);

SNX_RECOMPILER_API const char* snx_recompiler_error(const struct snx_recompiler* r);

SNX_RECOMPILER_API int snx_recompiler_set_optimization(struct snx_recompiler* r, char opt);

SNX_RECOMPILER_API void snx_recompiler_set_jobs(struct snx_recompiler* r, unsigned jobs);

SNX_RECOMPILER_API void snx_recompiler_set_verbose(struct snx_recompiler* r, int verbose);

SNX_RECOMPILER_API int snx_recompiler_set_keep_ir(struct snx_recompiler* r, const char* folder);

SNX_RECOMPILER_API int snx_recompiler_set_cache(struct snx_recompiler* r, const char* folder);




typedef void (*snx_svc_call64)(CPU* cpu, uint32_t imm);


SNX_RECOMPILER_API void snx_runtime_defaults(RtAPI* rt);


struct snx_code_range {
    uint64_t address;
    const void* code;
    size_t size;
};

struct snx_recompiled;


SNX_RECOMPILER_API struct snx_recompiled* snx_recompile(struct snx_recompiler* r, uint64_t address, const void* code,
                                                        size_t size);

SNX_RECOMPILER_API struct snx_recompiled* snx_recompile_ranges(struct snx_recompiler* r,
                                                               const struct snx_code_range* ranges, size_t range_count,
                                                               const uint64_t* hints, size_t hint_count);

SNX_RECOMPILER_API struct snx_recompiled* snx_recompile_from(struct snx_recompiler* r,
                                                             const struct snx_code_range* ranges, size_t range_count,
                                                             const uint64_t* hints, size_t hint_count, size_t limit,
                                                             int (*skip)(void* context, uint64_t address),
                                                             void* context);

SNX_RECOMPILER_API void snx_recompiled_free(struct snx_recompiled* m);

SNX_RECOMPILER_API void snx_recompiled_range(const struct snx_recompiled* m, uint64_t* code_lo, uint64_t* code_hi);

SNX_RECOMPILER_API size_t snx_recompiled_functions(const struct snx_recompiled* m);
SNX_RECOMPILER_API size_t snx_recompiled_fallbacks(const struct snx_recompiled* m);

SNX_RECOMPILER_API int snx_recompiled_bind(struct snx_recompiled* m, CPU* cpu);

SNX_RECOMPILER_API GuestFn snx_recompiled_lookup(const struct snx_recompiled* m, uint64_t pc);

SNX_RECOMPILER_API GuestFn snx_recompiled_function(const struct snx_recompiled* m, uint64_t pc);

SNX_RECOMPILER_API void snx_recompiled_set_fallback(struct snx_recompiled* m, GuestFn fallback);

SNX_RECOMPILER_API void snx_recompiled_call(struct snx_recompiled* m, CPU* cpu, uint64_t pc);

SNX_RECOMPILER_API void snx_recompiled_run(struct snx_recompiled* m, CPU* cpu, uint64_t pc, uint64_t stop);




typedef int (*snx_svc_call32)(void* context, uint32_t imm, uint64_t number, const uint64_t arguments[6]);

struct snx_recompiled32;


SNX_RECOMPILER_API struct snx_recompiled32* snx_recompile32(struct snx_recompiler* r, uint64_t address, size_t size);
SNX_RECOMPILER_API struct snx_recompiled32*
snx_recompile32_ranges(struct snx_recompiler* r, const struct snx_code_range* ranges, size_t range_count);
SNX_RECOMPILER_API void snx_recompiled32_free(struct snx_recompiled32* m);

SNX_RECOMPILER_API int snx_recompiled32_run(struct snx_recompiled32* m, SnxArmCPU* cpu, snx_svc_call32 svc,
                                            void* context);

#ifdef __cplusplus
}
#endif
#endif
