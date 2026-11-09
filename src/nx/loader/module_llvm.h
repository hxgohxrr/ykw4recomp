/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_NX_MODULE_LLVM_H
#define SNX_NX_MODULE_LLVM_H

#include "nx/loader/module.h"

typedef struct {
    const char* output;  
    char opt;            
    unsigned jobs;
    int quiet;
    const char* keep_ir;  
} SnxLlvmBuild;


unsigned snx_module_abi(void);

int snx_build_module_llvm(SnxProgram* loaded, const SnxCodegenOptions* options, const SnxLlvmBuild* build);

int snx_build_module_gccjit(SnxProgram* loaded, const SnxCodegenOptions* options, const SnxLlvmBuild* build);

int snx_build_module_arm(SnxProgram* loaded, const SnxCodegenOptions* options, const SnxLlvmBuild* build);

#endif
