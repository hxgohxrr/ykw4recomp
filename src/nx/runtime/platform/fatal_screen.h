/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_FATAL_SCREEN_H
#define SNX_FATAL_SCREEN_H
#include <stddef.h>

#define FATAL_ROW 1024  


const char* fatal_hint(const char* msg);

int fatal_wrap(const char* text, int cols, char (*rows)[FATAL_ROW], int max);

void fatal_cp437(const char* text, char* out, size_t size);

int fatal_log_url(const char* path, char* out, size_t size);

int fatal_share_log(const char* folder, const char* build, const char* msg, const char* game_log, char* saved,
                    size_t size);
#endif
