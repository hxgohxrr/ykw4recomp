/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GAME_NAME_H
#define SNX_GAME_NAME_H
#include "nx/loader/loader.h"


char* snx_game_title(const char* name, const SnxNpdm* npdm, const char* input);

char* snx_game_name(const char* name, const SnxNpdm* npdm, const char* input);

char* snx_game_shown(const char* name, const SnxProgram* program);
#endif
