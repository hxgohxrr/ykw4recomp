/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_NX_MODULE_IR_H
#define SNX_NX_MODULE_IR_H

#include "nx/loader/module_llvm.h"
#include "common/cpu/recompiler/llvm/ir.h"


LLVMTypeRef snx_module_info_type(SnxIr* ir, char* error, size_t error_size);
LLVMValueRef snx_module_mods(SnxIr* ir, LLVMTypeRef info, const SnxProgram* loaded);
LLVMValueRef snx_module_string(SnxIr* ir, const char* text);

#endif
