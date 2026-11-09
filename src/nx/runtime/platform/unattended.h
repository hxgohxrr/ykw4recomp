/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_UNATTENDED_H
#define SNX_UNATTENDED_H
#include "common/cpu/aarch64/cpu.h"
#include <stddef.h>

int unattended_ini(const char* k, const char* v);  
int unattended_known(const char* k);               
void unattended_start(void);                       
int unattended_active(void);                       

u64 unattended_buttons(void);
const char* unattended_shot(void);  

int unattended_periodic_shot(char* path, size_t n);
int unattended_done(void);  

__attribute__((noreturn)) void unattended_exit(u64 hash, int w, int h);


void input_key_sticks(u64 k, i32 st[4]);

void input_stick_clamp(i32* x, i32* y);

#endif
