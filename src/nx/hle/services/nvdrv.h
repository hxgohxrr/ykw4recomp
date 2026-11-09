/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_NVDRV_H
#define SNX_NVDRV_H
#include "common/cpu/aarch64/cpu.h"

void nv_init(void);

int nvmap_get(u32 h, u64* addr, u32* size);
u64 nvmap_addr_by_id(u32 id);
u64 gmmu_translate(u64 gva);
u64 gmmu_run(u64 gva, u64 max, u64* cpu);
u32 syncpt_alloc(void);    
void syncpt_free(u32 id);  

#define SYNCPT_IDS 256
u32 syncpt_read(u32 id);
void syncpt_incr(u32 id);
void syncpt_add(u32 id, u32 n);  
int syncpt_wait(u32 id, u32 thresh, i64 timeout_ns);

void nv_gpu_call_after(u32 n, u32* id, const u32* val, void (*fn)(void*), void* arg);

#endif
