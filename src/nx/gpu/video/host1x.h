/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_HOST1X_H
#define SNX_HOST1X_H
#include "common/cpu/aarch64/cpu.h"


typedef struct H1Ch H1Ch;
H1Ch* host1x_open(int is_vic);  

void host1x_close(H1Ch* ch);
u32 host1x_ioctl(H1Ch* ch, u32 cmd, u8* d, u32 n);

void host1x_forget_handle(u32 h);

void ffmpeg_log_init(void);

#endif
