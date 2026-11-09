/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_OPTIONS_H
#define SNX_OPTIONS_H
#include <stddef.h>

#include "nx/runtime/startup.h"

extern int g_opt_fullscreen;  

void ini_override(const char* k, const char* v);  
void ini_set(const char* k, const char* v);

void ini_list(void);

int snx_setting_known(const char* key);
void ini_open(const char* path);  

void ini_poll(int final);
extern int g_opt_docked, g_opt_lang;

#define SNX_FPS_GAME (-1)
extern int g_opt_fps;
void fps_limit_text(int fps, char* out, size_t n);  
extern int g_opt_diag, g_opt_shadows, g_opt_prof;
extern int g_opt_gpu_sync_trace;

extern int g_opt_auto_stub_ipc;
extern int g_opt_network;  
extern float g_opt_resf;
extern int g_opt_vsync;  
extern int g_opt_aniso;  
extern int g_opt_backface_culling;

#endif
