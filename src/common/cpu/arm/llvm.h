/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_ARM_LLVM_H
#define SNX_ARM_LLVM_H
#include "common/cpu/arm/arm.h"
#include "common/cpu/arm/checked.h"
#include <llvm-c/Core.h>


LLVMValueRef snx_arm_llvm_block(LLVMModuleRef module, const char* name, const uint32_t* words, size_t count,
                                uint32_t pc, size_t* consumed);

LLVMValueRef snx_thumb_llvm_block(LLVMModuleRef module, const char* name, const uint16_t* halfwords, size_t count,
                                  uint32_t pc, size_t* consumed);


typedef enum { SNX_ARM_MEMORY_FLAT, SNX_ARM_MEMORY_CHECKED } SnxArmMemoryMode;
#define SNX_ARM_CODEGEN_OPTIONS_ABI 1u
typedef struct {
    uint32_t abi;
    SnxArmProfile profile;
    SnxArmMemoryMode memory;
} SnxArmCodegenOptions;
LLVMValueRef snx_arm_llvm_block_ex(LLVMModuleRef module, const char* name, const uint32_t* words, size_t count,
                                   uint32_t pc, size_t* consumed, const SnxArmCodegenOptions* options);
LLVMValueRef snx_thumb_llvm_block_ex(LLVMModuleRef module, const char* name, const uint16_t* halfwords, size_t count,
                                     uint32_t pc, size_t* consumed, const SnxArmCodegenOptions* options);
#endif
