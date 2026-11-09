/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_STARTUP_H
#define SNX_STARTUP_H

int snx_startup_options(int argc, char** argv);  
extern int g_opt_headless;                       
extern int g_opt_gdb, g_opt_gdb_stop;
extern int g_cli_fullscreen;  

#endif
