/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_INI_LINES_H
#define SNX_INI_LINES_H

typedef struct {
    char* at;          
    char section[32];  
} SnxIni;


void snx_ini_start(SnxIni* ini, char* text);

int snx_ini_next(SnxIni* ini, char** key, char** value);
#endif
