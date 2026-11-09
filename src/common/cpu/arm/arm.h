/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_ARM_H
#define SNX_ARM_H

#include <stddef.h>
#include <stdint.h>


#define SNX_ARM_ABI 3  
#define SNX_ARM_T   (1u << 5)
#define SNX_ARM_E   (1u << 9)

typedef enum {
    SNX_ARM_OK,
    SNX_ARM_UNDEFINED,
    SNX_ARM_THUMB16_UNSUPPORTED,
    SNX_ARM_THUMB32_UNSUPPORTED,
    SNX_ARM_SVC,
    SNX_ARM_BREAKPOINT,
    SNX_ARM_DATA_ABORT,
    SNX_ARM_ALIGNMENT,
    SNX_ARM_BAD_ENTRY,
    SNX_ARM_ENDIAN_UNSUPPORTED,
    SNX_ARM_BACKEND_UNSUPPORTED,
    SNX_ARM_PREFETCH_ABORT,
    SNX_ARM_CODE_CHANGED
} SnxArmExit;

struct SnxArmRt;
typedef struct {
    uint32_t r[16];           
    uint32_t cpsr;            
    uint32_t exit, syndrome;  
    uint8_t* mem;
    uint64_t mem_size;  
    uint64_t exclusive_value;
    uint32_t exclusive_address, exclusive_size, exclusive_valid;
    uint32_t tpidrurw, tpidruro;  
    uint32_t fpscr;               
    uint64_t d[32];               
    const struct SnxArmRt* rt;    
} SnxArmCPU;


typedef struct SnxArmRt {

    void (*undefined)(SnxArmCPU* cpu, uint32_t word, uint32_t reason);

    void (*coproc)(SnxArmCPU* cpu, uint32_t word);
} SnxArmRt;


void snx_arm_undefined(SnxArmCPU* cpu, uint32_t word, uint32_t reason);
void snx_arm_coproc(SnxArmCPU* cpu, uint32_t word);

void snx_arm_coproc_user(SnxArmCPU* cpu, uint32_t word);

typedef void (*SnxArmBlock)(SnxArmCPU* cpu);

typedef enum {
    SNX_A32_UNDEFINED,
    SNX_A32_DATA,
    SNX_A32_SINGLE,
    SNX_A32_EXTRA,
    SNX_A32_MULTIPLE,
    SNX_A32_BRANCH,
    SNX_A32_BX,
    SNX_A32_BLX_IMM,
    SNX_A32_MOVW,
    SNX_A32_MOVT,
    SNX_A32_MUL,
    SNX_A32_MULL,
    SNX_A32_MLS,
    SNX_A32_DIV,
    SNX_A32_CLZ,
    SNX_A32_REV,
    SNX_A32_EXTEND,
    SNX_A32_BITFIELD,
    SNX_A32_MRS,
    SNX_A32_MSR,
    SNX_A32_EXCLUSIVE,
    SNX_A32_ACQUIRE_RELEASE,
    SNX_A32_CLREX,
    SNX_A32_BARRIER,
    SNX_A32_HINT,
    SNX_A32_SVC,
    SNX_A32_BKPT,
    SNX_A32_TLS,
    SNX_A32_VECTOR,
    SNX_A32_UADD8,
    SNX_A32_SEL,
    SNX_A32_MULH,  
    SNX_A32_COPROC,  
    SNX_A32_SATURATE,    
    SNX_A32_PACK,        
    SNX_A32_SELECT,      
    SNX_A32_PARALLEL,    
    SNX_A32_QARITH,      
    SNX_A32_HALF_MUL,    
    SNX_A32_DUAL_MUL,    
    SNX_A32_ABS_DIFF,    
    SNX_A32_CRC32,       
    SNX_A32_SETEND       
} SnxA32Kind;


SnxA32Kind snx_a32_decode(uint32_t word);


typedef enum { SNX_ARM_PROFILE_A57, SNX_ARM_PROFILE_ARM11 } SnxArmProfile;
SnxA32Kind snx_a32_decode_profile(SnxArmProfile profile, uint32_t word);


typedef enum {
    TH_UNDEF,
    TH_SHIFT_IMM,  
    TH_ADDSUB,     
    TH_IMM8,       
    TH_ALU,        
    TH_HI,         
    TH_LOAD,       
    TH_STORE,      
    TH_ADDR,       
    TH_EXTEND,     
    TH_PUSHPOP,    
    TH_LDMSTM,     
    TH_BRANCH,     
    TH_MUL,        
    TH_BITFIELD,   
    TH_WIMM,       
    TH_WDATA,      
    TH_TABLE,      
    TH_IT,         
    TH_HINT,       
    TH_LEGACY_BL,  
    TH_REV,        
    TH_A32,       /* a Thumb32 instruction that is an A32 one in another encoding: .imm is the A32 word (condition
                     AL) and .op its SnxA32Kind, emitted by the A32 emitter with Thumb's PC. .ra is a doubleword
                     exclusive's second register + 1 (A32's is the first + 1), else 0. Coprocessor, VFP and Advanced
                     SIMD, LDM/STM, exclusives, barriers, MRS/MSR, long multiplies and divides, REV and CLZ */
    TH_DUAL        
} SnxThumbShape;


enum {
    ALU_AND,
    ALU_EOR,
    ALU_LSL,
    ALU_LSR,
    ALU_ASR,
    ALU_ADC,
    ALU_SBC,
    ALU_ROR,
    ALU_TST,
    ALU_NEG,
    ALU_CMP,
    ALU_CMN,
    ALU_ORR,
    ALU_MUL,
    ALU_BIC,
    ALU_MVN,
    ALU_ADD,
    ALU_SUB,
    ALU_RSB,
    ALU_ORN
};

enum { BR_B, BR_BCOND, BR_BL, BR_BLX_IMM, BR_BX, BR_BLX_REG, BR_CBZ, BR_CBNZ, BR_BW, BR_SVC, BR_BKPT, BR_BCOND_W };

enum { I8_MOV, I8_CMP, I8_ADD, I8_SUB };


#define TH_B      (1u << 0)   
#define TH_H      (1u << 1)   
#define TH_SIGN   (1u << 2)   
#define TH_SPB    (1u << 3)   
#define TH_PCB    (1u << 4)   
#define TH_PRE    (1u << 5)   
#define TH_UP     (1u << 6)   
#define TH_WB     (1u << 7)   
#define TH_S      (1u << 8)   
#define TH_REG    (1u << 9)   
#define TH_RSHIFT (1u << 10)  
#define TH_MODIMM (1u << 11)  
#define TH_ROTC   (1u << 12)  

#define TH_F16 1u  
#define TH_F32 2u
typedef struct {
    SnxThumbShape shape;
    unsigned op;  
    unsigned rd, rn, rm, ra;
    uint32_t imm;
    unsigned flags;
    unsigned len;  
} SnxThumb;


unsigned snx_thumb_decode(uint16_t hw1, uint16_t hw2, SnxThumb* out);
unsigned snx_thumb_decode_profile(SnxArmProfile profile, uint16_t hw1, uint16_t hw2, SnxThumb* out);

SnxArmExit snx_arm_gccjit_stub(void);

#endif
