/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_COMPILE_H
#define SNX_CTR_COMPILE_H
#include <stddef.h>

int snx_ctr_aot_compile(const char* extracted, char* error, size_t error_size);
#endif
