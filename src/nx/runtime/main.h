/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_MAIN_H
#define SNX_MAIN_H
#include "common/cpu/aarch64/cpu.h"

int snx_main(int argc, char** argv);

extern int (*snx_menu)(const char* self);

void snx_save_before_exit(void);
extern char g_datadir[1024], g_exename[256], g_exedir[1024];

extern char g_confdir[1024], g_statedir[1024], g_cachedir[1024];
extern volatile int g_quit;
extern int g_frames;
void crash_report(void* host_pc, void* fault, int sig);

void vi_show_buffer(u32 nvmap_id, u32 offset, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, const i32 crop[4],
                    u32 transform);

void present_pace(u32 interval);
void vi_convert_cpu(u64 a, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, u32* out);

#endif
