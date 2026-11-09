/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_HOST1X_INT_H
#define SNX_HOST1X_INT_H
#include "nx/gpu/video/host1x.h"
#include "nx/gpu/gpu.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include <stdlib.h>
#include <string.h>


static inline u32 rd32(const u8* p, u32 off) {
    u32 v;
    memcpy(&v, p + off, 4);
    return v;
}
static inline u64 rd64(const u8* p, u32 off) {
    u64 v;
    memcpy(&v, p + off, 8);
    return v;
}


u8* iova_ptr(u64 iova, u64 len);


typedef struct NvdecCtx NvdecCtx;
NvdecCtx* nvdec_new(void);  
void nvdec_free(NvdecCtx* d);
void nvdec_execute(NvdecCtx* d, const u32* r);


void vic_execute(const u32* r);

#endif
