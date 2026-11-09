/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_RECOMPILE_H
#define SNX_RECOMPILE_H
#include "common/utilities/common.h"

unsigned snx_game_abi(void);

char* snx_game_stamp(void);
int snx_recompile(int argc, char** argv, const SnxPaths* paths);
#endif
