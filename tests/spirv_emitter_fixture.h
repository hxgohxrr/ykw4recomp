/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_SPIRV_EMITTER_FIXTURE_H
#define SNX_SPIRV_EMITTER_FIXTURE_H
#include <stddef.h>
#include <stdint.h>


uint32_t* snx_test_spirv_compute(size_t* words);

uint32_t* snx_test_spirv_graphics(int fragment, size_t* words);
uint32_t* snx_test_spirv_culling(int fragment, size_t* words);
#endif
