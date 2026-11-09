/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_RECOMPILER_GCCJIT_JIT_H
#define SNX_RECOMPILER_GCCJIT_JIT_H

#include "common/cpu/recompiler/codegen.h"
#include <libgccjit.h>

typedef struct {
    const char* name;
    gcc_jit_function* fn;
} SnxJitExtern;

typedef struct SnxJit {
    gcc_jit_context* ctx;
    gcc_jit_type *t_void, *t_ptr, *t_char_ptr, *t_bool, *t_int, *t_guest, *t_guest_ptr;
    gcc_jit_type *t_u8, *t_u16, *t_u32, *t_u64, *t_u128, *t_s8, *t_s16, *t_s32, *t_s64, *t_s128, *t_f32, *t_f64;
    gcc_jit_function* lookup;  
    gcc_jit_lvalue* interp_entry;

    SnxJitExtern externs[64];
    size_t extern_count;

    SnxU64* addresses;
    gcc_jit_function** functions;
    size_t count, capacity;

    int fma, round;

    gcc_jit_function* fn;
    gcc_jit_rvalue *c, *M;
    gcc_jit_block* block;  
    unsigned long names;

    unsigned long fallbacks;
} SnxJit;


SnxJit* snx_jit_new(char opt);
void snx_jit_free(SnxJit* jit);

const char* snx_jit_error(SnxJit* jit);


gcc_jit_function* snx_jit_guest_function(SnxJit* jit, SnxU64 address, int define);
void snx_jit_emit(SnxJit* jit, SnxCodegen* codegen, const SnxFunction* function);

gcc_jit_function* snx_jit_static_step(SnxJit* jit, SnxU32 word, SnxU64 pc, const char* name);

void snx_jit_host(void);


typedef struct SnxJitUnits {
    SnxCodegen* codegen;
    const size_t* bounds;
    char* const* objects;
    size_t count;
    char opt;             
    unsigned jobs;        
    const char* keep_ir;  

    void (*done)(const struct SnxJitUnits* units, size_t unit, size_t finished, double seconds);
    void* user;
    unsigned long fallbacks;  
    char error[1024];
} SnxJitUnits;

int snx_jit_compile_units(SnxJitUnits* units);

const char* snx_jit_compile_to_file(SnxJit* jit, enum gcc_jit_output_kind kind, const char* path);
gcc_jit_result* snx_jit_compile(SnxJit* jit);



int snx_jit_plain(SnxJit* jit, SnxU32 word, SnxU64 pc, const SnxA64Case* instruction);

gcc_jit_rvalue* snx_jit_flow_operand(SnxJit* jit, SnxU32 word, SnxU64 pc, const SnxA64Case* instruction);
int snx_jit_fp_plain(SnxJit* jit, SnxU32 word, SnxU64 pc, const SnxA64Pattern* pattern, size_t index);
int snx_jit_simd_plain(SnxJit* jit, SnxU32 word, SnxU64 pc, const SnxA64Pattern* pattern, size_t index);
int snx_jit_fast_simd(SnxJit* jit, SnxU32 word);
int snx_jit_more_simd(SnxJit* jit, SnxU32 word);


enum { SNX_JIT_FP_ADD, SNX_JIT_FP_SUB, SNX_JIT_FP_MUL, SNX_JIT_FP_DIV, SNX_JIT_FP_SQRT, SNX_JIT_FP_FMA };
typedef struct {
    gcc_jit_rvalue* value[4];  
    gcc_jit_rvalue* ok;        
    gcc_jit_rvalue* inexact;   
} SnxJitFpFast;

void jit_fp_fast(SnxJit* jit, unsigned op, unsigned width, unsigned lanes, gcc_jit_rvalue* const* a,
                 gcc_jit_rvalue* const* b, gcc_jit_rvalue* const* add, SnxJitFpFast* out);

gcc_jit_block* jit_fp_branch(SnxJit* jit, gcc_jit_rvalue* ok, gcc_jit_block** slow);
void jit_fp_else(SnxJit* jit, gcc_jit_block* join, gcc_jit_block* slow, gcc_jit_rvalue* inexact);
void jit_fp_join(SnxJit* jit, gcc_jit_block* join);

gcc_jit_rvalue* jit_fp_binary(SnxJit* jit, gcc_jit_rvalue* a, gcc_jit_rvalue* b, unsigned width, unsigned op);
gcc_jit_rvalue* jit_fp_fma(SnxJit* jit, gcc_jit_rvalue* add, gcc_jit_rvalue* a, gcc_jit_rvalue* b, unsigned width);

void jit_fp_minmax_fast(SnxJit* jit, unsigned width, unsigned lanes, gcc_jit_rvalue* const* a, gcc_jit_rvalue* const* b,
                        unsigned minimum, SnxJitFpFast* out);

gcc_jit_rvalue* jit_fp_minmax(SnxJit* jit, gcc_jit_rvalue* a, gcc_jit_rvalue* b, unsigned width, unsigned minimum,
                              unsigned numeric);

enum { SNX_JIT_CMP_EQ, SNX_JIT_CMP_GE, SNX_JIT_CMP_GT, SNX_JIT_CMP_LE, SNX_JIT_CMP_LT };

void jit_fp_compare_fast(SnxJit* jit, unsigned cmp, unsigned width, unsigned lanes, gcc_jit_rvalue* const* a,
                         gcc_jit_rvalue* const* b, SnxJitFpFast* out);

gcc_jit_rvalue* jit_fp_compare_mask(SnxJit* jit, unsigned cmp, gcc_jit_rvalue* a, gcc_jit_rvalue* b, unsigned width);

void jit_fp_from_int_fast(SnxJit* jit, unsigned width, unsigned lanes, gcc_jit_rvalue* const* v, unsigned integer,
                          unsigned uns, unsigned fbits, SnxJitFpFast* out);

int jit_fp_to_int_fast(SnxJit* jit, unsigned width, unsigned lanes, gcc_jit_rvalue* const* bits, unsigned integer,
                       unsigned uns, unsigned rm, unsigned fbits, SnxJitFpFast* out);

void jit_fp_estimate_fast(SnxJit* jit, unsigned rsqrt, unsigned width, unsigned lanes, gcc_jit_rvalue* const* bits,
                          SnxJitFpFast* out);

void jit_fp_step_fast(SnxJit* jit, unsigned rsqrt, unsigned width, unsigned lanes, gcc_jit_rvalue* const* a,
                      gcc_jit_rvalue* const* b, SnxJitFpFast* out);

void jit_fp_narrow_fast(SnxJit* jit, unsigned from, unsigned lanes, gcc_jit_rvalue* const* bits, unsigned odd,
                        SnxJitFpFast* out);

gcc_jit_rvalue* jit_fp_narrow(SnxJit* jit, gcc_jit_rvalue* bits, unsigned from, unsigned odd);

gcc_jit_rvalue* jit_fp_unary(SnxJit* jit, gcc_jit_rvalue* bits, unsigned width, unsigned op);
gcc_jit_rvalue* jit_fp_to_int(SnxJit* jit, gcc_jit_rvalue* bits, unsigned fp, unsigned integer, unsigned uns,
                              unsigned fbits, unsigned rm);
gcc_jit_rvalue* jit_fp_from_int(SnxJit* jit, gcc_jit_rvalue* bits, unsigned integer, unsigned fp, unsigned uns,
                                unsigned fbits);


gcc_jit_rvalue* jit_k(SnxJit* jit, gcc_jit_type* type, SnxU64 v);
gcc_jit_rvalue* jit_u64(SnxJit* jit, SnxU64 v);
gcc_jit_rvalue* jit_u32(SnxJit* jit, SnxU32 v);
gcc_jit_rvalue* jit_int(SnxJit* jit, int v);
gcc_jit_rvalue* jit_true(SnxJit* jit);
gcc_jit_rvalue* jit_false(SnxJit* jit);
gcc_jit_type* jit_uint(SnxJit* jit, unsigned bits);  
gcc_jit_type* jit_sint(SnxJit* jit, unsigned bits);  
gcc_jit_rvalue* jit_cast(SnxJit* jit, gcc_jit_rvalue* v, gcc_jit_type* type);
gcc_jit_rvalue* jit_bitcast(SnxJit* jit, gcc_jit_rvalue* v, gcc_jit_type* type);
gcc_jit_rvalue* jit_val(SnxJit* jit, gcc_jit_rvalue* v);
gcc_jit_lvalue* jit_local(SnxJit* jit, gcc_jit_type* type);
void jit_assign(SnxJit* jit, gcc_jit_lvalue* lvalue, gcc_jit_rvalue* value);  
gcc_jit_rvalue* jit_op(SnxJit* jit, enum gcc_jit_binary_op op, gcc_jit_rvalue* a, gcc_jit_rvalue* b);
gcc_jit_rvalue* jit_unary(SnxJit* jit, enum gcc_jit_unary_op op, gcc_jit_rvalue* a);
gcc_jit_rvalue* jit_cmp(SnxJit* jit, enum gcc_jit_comparison op, gcc_jit_rvalue* a, gcc_jit_rvalue* b);
gcc_jit_rvalue* jit_land(SnxJit* jit, gcc_jit_rvalue* a, gcc_jit_rvalue* b);
gcc_jit_rvalue* jit_lor(SnxJit* jit, gcc_jit_rvalue* a, gcc_jit_rvalue* b);
gcc_jit_rvalue* jit_lnot(SnxJit* jit, gcc_jit_rvalue* a);

gcc_jit_rvalue* jit_sel(SnxJit* jit, gcc_jit_rvalue* cond, gcc_jit_rvalue* a, gcc_jit_rvalue* b);

gcc_jit_rvalue* jit_expect(SnxJit* jit, gcc_jit_rvalue* cond, int likely);


gcc_jit_lvalue* jit_at(SnxJit* jit, gcc_jit_rvalue* base, size_t offset, gcc_jit_type* type);
gcc_jit_lvalue* jit_cpu(SnxJit* jit, size_t offset, gcc_jit_type* type);
gcc_jit_rvalue* jit_ld(SnxJit* jit, size_t offset, gcc_jit_type* type);
void jit_st(SnxJit* jit, size_t offset, gcc_jit_type* type, gcc_jit_rvalue* value);
size_t jit_off_x(unsigned n);
size_t jit_off_v(unsigned n);
gcc_jit_rvalue* jit_x(SnxJit* jit, unsigned n);                  
gcc_jit_rvalue* jit_rx(SnxJit* jit, unsigned n);                 
void jit_set_x(SnxJit* jit, unsigned n, gcc_jit_rvalue* value);  
void jit_setx(SnxJit* jit, unsigned n, gcc_jit_rvalue* value);   
gcc_jit_rvalue* jit_vd(SnxJit* jit, unsigned n, unsigned lane);  
void jit_set_vd(SnxJit* jit, unsigned n, unsigned lane, gcc_jit_rvalue* value);

gcc_jit_rvalue* jit_ve(SnxJit* jit, unsigned n, unsigned size, unsigned index);
void jit_set_ve(SnxJit* jit, unsigned n, unsigned size, unsigned index, gcc_jit_rvalue* value);


gcc_jit_rvalue* jit_rd(SnxJit* jit, gcc_jit_rvalue* address, unsigned bits);
void jit_wr(SnxJit* jit, gcc_jit_rvalue* address, unsigned bits, gcc_jit_rvalue* value);
gcc_jit_rvalue* jit_ldacq(SnxJit* jit, gcc_jit_rvalue* address, unsigned bits);
void jit_strel(SnxJit* jit, gcc_jit_rvalue* address, unsigned bits, gcc_jit_rvalue* value);

gcc_jit_rvalue* jit_cas(SnxJit* jit, gcc_jit_rvalue* address, unsigned bits, gcc_jit_rvalue* expected,
                        gcc_jit_rvalue* value);
gcc_jit_rvalue* jit_mp(SnxJit* jit, gcc_jit_rvalue* address);  


gcc_jit_function* jit_extern(SnxJit* jit, const char* name, gcc_jit_type* ret, int count, gcc_jit_type** params);
gcc_jit_function* jit_builtin(SnxJit* jit, const char* name);
gcc_jit_rvalue* jit_call(SnxJit* jit, gcc_jit_function* function, int count, gcc_jit_rvalue** args);

gcc_jit_rvalue* jit_rt_call(SnxJit* jit, size_t offset, gcc_jit_type* ret, int count, gcc_jit_type** params,
                            gcc_jit_rvalue** args);
size_t jit_rt_sev(void);
size_t jit_rt_wfe(void);
void jit_undef(SnxJit* jit, SnxU64 pc, SnxU32 word);  
void jit_fence(SnxJit* jit);                          
void jit_barrier(SnxJit* jit);                        


gcc_jit_block* jit_block(SnxJit* jit);
void jit_goto(SnxJit* jit, gcc_jit_block* target);
void jit_branch(SnxJit* jit, gcc_jit_rvalue* cond, gcc_jit_block* yes, gcc_jit_block* no);
void jit_enter(SnxJit* jit, gcc_jit_block* block);

#endif
