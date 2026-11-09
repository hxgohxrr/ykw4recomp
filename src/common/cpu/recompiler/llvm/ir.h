/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_RECOMPILER_LLVM_IR_H
#define SNX_RECOMPILER_LLVM_IR_H

#include "common/cpu/recompiler/codegen.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>


enum {
    SNX_IR_ADDS64,
    SNX_IR_ADDS32,
    SNX_IR_SUBS64,
    SNX_IR_SUBS32,
    SNX_IR_LOGIC64,
    SNX_IR_LOGIC32,
    SNX_IR_COND_OK,
    SNX_IR_SET_NZCV,
    SNX_IR_ROR64,
    SNX_IR_ROR32,
    SNX_IR_SHF64,
    SNX_IR_SHF32,
    SNX_IR_EXT64,
    SNX_IR_CLZ64,
    SNX_IR_CLZ32,
    SNX_IR_CLS64,
    SNX_IR_CLS32,
    SNX_IR_RBIT64,
    SNX_IR_RBIT32,
    SNX_IR_REV16_64,
    SNX_IR_REV32_64,
    SNX_IR_BSWAP32,
    SNX_IR_BSWAP64,
    SNX_IR_UDIV64,
    SNX_IR_UDIV32,
    SNX_IR_SDIV64,
    SNX_IR_SDIV32,
    SNX_IR_UMULH64,
    SNX_IR_SMULH64,
    SNX_IR_BMASK,
    SNX_IR_UBFM,
    SNX_IR_SBFM,
    SNX_IR_BFM,
    SNX_IR_LDX,
    SNX_IR_STX,
    SNX_IR_LDXP,
    SNX_IR_STXP,
    SNX_IR_LOOKUP,
    SNX_IR_DC_ZVA,
    SNX_IR_VFP_IMM32,
    SNX_IR_VFP_IMM64,
    SNX_IR_HELPER_COUNT
};


typedef struct {
    int x86;                                   
    int sse42, crc32, pclmul, aes, gfni, fma;  
    int avx2, avx512bw, avx512vl, avx512bitalg;
} SnxIrFeatures;
extern SnxIrFeatures snx_ir_host;

typedef struct SnxIr {
    LLVMContextRef ctx;
    LLVMModuleRef mod;
    LLVMBuilderRef b;
    LLVMTargetMachineRef tm;
    int windows;
    const SnxIrFeatures* features;
    LLVMTypeRef t_void, t_i1, t_i8, t_i16, t_i32, t_i64, t_i128, t_ptr, t_guest;

    LLVMValueRef fn, c, M;
    LLVMValueRef helpers[SNX_IR_HELPER_COUNT];

    unsigned kind_scope, kind_noalias;
    LLVMValueRef scope_cpu, scope_guest;

    unsigned long fallbacks;
} SnxIr;


int snx_ir_init(char* error, size_t error_size);

SnxIr* snx_ir_new(const char* name, char opt, char* error, size_t error_size);

SnxIr* snx_ir_new_target(const char* name, char opt, const char* triple, char* error, size_t error_size);
void snx_ir_free(SnxIr* ir);

int snx_ir_object(SnxIr* ir, char opt, unsigned char** data, size_t* size, char* error, size_t error_size);

int snx_ir_rtlib_object(char opt, unsigned char** data, size_t* size, char* error, size_t error_size);

int snx_ir_armlib_object(char opt, unsigned char** data, size_t* size, char* error, size_t error_size);

char* snx_ir_text(SnxIr* ir);



LLVMValueRef snx_ir_guest_function(SnxIr* ir, SnxU64 address);
void snx_ir_emit(SnxIr* ir, SnxCodegen* codegen, const SnxFunction* function);

LLVMValueRef snx_ir_static_step(SnxIr* ir, SnxU32 word, SnxU64 pc, const char* name);

void snx_ir_begin(SnxIr* ir, LLVMValueRef function);



int snx_ir_plain(SnxIr* ir, SnxU32 word, SnxU64 pc, const SnxA64Case* instruction);

LLVMValueRef snx_ir_flow_operand(SnxIr* ir, SnxU32 word, SnxU64 pc, const SnxA64Case* instruction);
int snx_ir_simd_plain(SnxIr* ir, SnxU32 word, SnxU64 pc, const SnxA64Pattern* pattern, size_t index);
int snx_ir_fp_plain(SnxIr* ir, SnxU32 word, SnxU64 pc, const SnxA64Pattern* pattern, size_t index);

int snx_ir_fast_simd(SnxIr* ir, SnxU32 word);

int snx_ir_more_simd(SnxIr* ir, SnxU32 word);


enum { SNX_IR_FP_ADD, SNX_IR_FP_SUB, SNX_IR_FP_MUL, SNX_IR_FP_DIV, SNX_IR_FP_SQRT, SNX_IR_FP_FMA };
typedef struct {
    LLVMValueRef value;    
    LLVMValueRef ok;       
    LLVMValueRef inexact;  
} SnxFpFast;

void ir_fp_fast(SnxIr* ir, unsigned op, unsigned width, unsigned lanes, LLVMValueRef a, LLVMValueRef b,
                LLVMValueRef add, SnxFpFast* out);

LLVMBasicBlockRef ir_fp_branch(SnxIr* ir, LLVMValueRef ok, LLVMBasicBlockRef* slow);
void ir_fp_else(SnxIr* ir, LLVMBasicBlockRef join, LLVMBasicBlockRef slow, LLVMValueRef inexact);
void ir_fp_join(SnxIr* ir, LLVMBasicBlockRef join);

void ir_fp_minmax_fast(SnxIr* ir, unsigned width, unsigned lanes, LLVMValueRef a, LLVMValueRef b, unsigned minimum,
                       SnxFpFast* out);

LLVMValueRef ir_fp_minmax(SnxIr* ir, LLVMValueRef a, LLVMValueRef b, unsigned width, unsigned minimum,
                          unsigned numeric);

enum { SNX_IR_CMP_EQ, SNX_IR_CMP_GE, SNX_IR_CMP_GT, SNX_IR_CMP_LE, SNX_IR_CMP_LT };

void ir_fp_compare_fast(SnxIr* ir, unsigned cmp, unsigned width, unsigned lanes, LLVMValueRef a, LLVMValueRef b,
                        SnxFpFast* out);

LLVMValueRef ir_fp_compare_mask(SnxIr* ir, unsigned cmp, LLVMValueRef a, LLVMValueRef b, unsigned width);

void ir_fp_from_int_fast(SnxIr* ir, unsigned width, unsigned lanes, LLVMValueRef v, unsigned uns, unsigned fbits,
                         SnxFpFast* out);

int ir_fp_to_int_fast(SnxIr* ir, unsigned width, unsigned lanes, LLVMValueRef bits, unsigned integer, unsigned uns,
                      unsigned rm, unsigned fbits, SnxFpFast* out);

void ir_fp_estimate_fast(SnxIr* ir, unsigned rsqrt, unsigned width, unsigned lanes, LLVMValueRef bits, SnxFpFast* out);

void ir_fp_step_fast(SnxIr* ir, unsigned rsqrt, unsigned width, unsigned lanes, LLVMValueRef a, LLVMValueRef b,
                     SnxFpFast* out);

void ir_fp_narrow_fast(SnxIr* ir, unsigned from, unsigned lanes, LLVMValueRef bits, unsigned odd, SnxFpFast* out);

LLVMValueRef ir_fp_narrow(SnxIr* ir, LLVMValueRef bits, unsigned from, unsigned odd);

LLVMValueRef ir_fp_binary(SnxIr* ir, LLVMValueRef a, LLVMValueRef b, unsigned width, unsigned op);
LLVMValueRef ir_fp_unary(SnxIr* ir, LLVMValueRef bits, unsigned width, unsigned op);
LLVMValueRef ir_fp_to_int(SnxIr* ir, LLVMValueRef bits, unsigned fp, unsigned integer, unsigned uns, unsigned fbits,
                          unsigned rm);
LLVMValueRef ir_fp_from_int(SnxIr* ir, LLVMValueRef bits, unsigned integer, unsigned fp, unsigned uns, unsigned fbits);


LLVMValueRef ir_i8(SnxIr* ir, unsigned v);
LLVMValueRef ir_i16(SnxIr* ir, unsigned v);
LLVMValueRef ir_i32(SnxIr* ir, SnxU32 v);
LLVMValueRef ir_i64(SnxIr* ir, SnxU64 v);

LLVMValueRef ir_cast(SnxIr* ir, LLVMValueRef v, LLVMTypeRef to, int is_signed);
LLVMValueRef ir_u32(SnxIr* ir, LLVMValueRef v);             
LLVMValueRef ir_u64(SnxIr* ir, LLVMValueRef v);             
LLVMValueRef ir_s64(SnxIr* ir, LLVMValueRef v);             
LLVMValueRef ir_bool32(SnxIr* ir, LLVMValueRef condition);  


LLVMValueRef ir_cpu(SnxIr* ir, size_t offset);
LLVMValueRef ir_load(SnxIr* ir, LLVMTypeRef type, LLVMValueRef pointer);
void ir_store(SnxIr* ir, LLVMValueRef value, LLVMValueRef pointer);

LLVMValueRef ir_tag_cpu(SnxIr* ir, LLVMValueRef access);
LLVMValueRef ir_tag_guest(SnxIr* ir, LLVMValueRef access);
LLVMValueRef ir_cpu_load(SnxIr* ir, LLVMTypeRef type, size_t offset);
void ir_cpu_store(SnxIr* ir, LLVMValueRef value, size_t offset);
size_t ir_off_x(unsigned n);
size_t ir_off_v(unsigned n);
size_t ir_off_pc(void);
size_t ir_off_cf(void);
size_t ir_off_excl_on(void);
size_t ir_off_fpcr(void);
size_t ir_off_mem(void);
LLVMValueRef ir_x(SnxIr* ir, unsigned n);                  
void ir_set_x(SnxIr* ir, unsigned n, LLVMValueRef value);  
LLVMValueRef ir_rx(SnxIr* ir, unsigned n);                 
void ir_setx(SnxIr* ir, unsigned n, LLVMValueRef value);   
LLVMValueRef ir_vd(SnxIr* ir, unsigned n, unsigned lane);  
void ir_set_vd(SnxIr* ir, unsigned n, unsigned lane, LLVMValueRef value);

LLVMValueRef ir_ve(SnxIr* ir, unsigned n, unsigned size, unsigned index);
void ir_set_ve(SnxIr* ir, unsigned n, unsigned size, unsigned index, LLVMValueRef value);
LLVMValueRef ir_pc(SnxIr* ir);
void ir_set_pc(SnxIr* ir, LLVMValueRef value);


LLVMValueRef ir_mp(SnxIr* ir, LLVMValueRef address);
LLVMValueRef ir_rd(SnxIr* ir, LLVMValueRef address, unsigned bits);
void ir_wr(SnxIr* ir, LLVMValueRef address, unsigned bits, LLVMValueRef value);

LLVMValueRef ir_rd_bytes(SnxIr* ir, LLVMValueRef address, unsigned bytes);
void ir_wr_bytes(SnxIr* ir, LLVMValueRef address, LLVMValueRef value);
void ir_rdq(SnxIr* ir, LLVMValueRef destination, LLVMValueRef address);  
void ir_wrq(SnxIr* ir, LLVMValueRef address, LLVMValueRef source);       
LLVMValueRef ir_ldacq(SnxIr* ir, LLVMValueRef address, unsigned bits);   
void ir_strel(SnxIr* ir, LLVMValueRef address, unsigned bits, LLVMValueRef value);

LLVMValueRef ir_helper(SnxIr* ir, int id);
LLVMValueRef ir_call(SnxIr* ir, LLVMValueRef function, LLVMValueRef* args, unsigned count);
LLVMValueRef ir_call_helper(SnxIr* ir, int id, LLVMValueRef* args, unsigned count);

LLVMValueRef ir_extern(SnxIr* ir, const char* name, LLVMTypeRef ret, LLVMTypeRef* params, unsigned count);

LLVMValueRef ir_rt_call(SnxIr* ir, size_t offset, LLVMTypeRef ret, LLVMTypeRef* params, LLVMValueRef* args,
                        unsigned count);
size_t ir_rt_svc(void);
size_t ir_rt_undef(void);
size_t ir_rt_note_hint(void);
size_t ir_rt_brk(void);
size_t ir_rt_sev(void);
size_t ir_rt_wfe(void);
size_t ir_rt_poll(void);
size_t ir_off_poll(void);                          
void ir_undef(SnxIr* ir, SnxU64 pc, SnxU32 word);  
void ir_fence(SnxIr* ir);                          
void ir_yield(SnxIr* ir);                          

void ir_unlikely(SnxIr* ir, LLVMValueRef branch);

LLVMValueRef ir_intrinsic(SnxIr* ir, const char* name, LLVMTypeRef type, LLVMValueRef* args, unsigned count);

void ir_pure(SnxIr* ir, LLVMValueRef function);
void ir_argmem(SnxIr* ir, LLVMValueRef function);

LLVMBasicBlockRef ir_block(SnxIr* ir, const char* name);

LLVMValueRef ir_local(SnxIr* ir, LLVMTypeRef type);
LLVMTypeRef ir_v128(SnxIr* ir);  

#endif
