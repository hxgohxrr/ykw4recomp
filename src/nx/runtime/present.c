/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/runtime/frame_profile.h"
#include "nx/runtime/main.h"
#include "nx/runtime/options.h"
#include "nx/gpu/gpu.h"
#include "nx/gpu/renderer.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/hle/services/vi.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include <stdlib.h>
#include <string.h>

void vi_convert_cpu(u64 a, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, u32* out) {
    u32 bpp = (fmt == 4 || fmt == 6 || fmt == 7) ? 2 : 4;

    static __thread u8* lin;
    static __thread size_t lin_cap;
    if ((size_t)w * h * bpp > lin_cap) {
        free(lin);
        lin = malloc((size_t)w * h * bpp);
        if (!lin) fatal("Out of memory converting a %ux%u frame", w, h);
        lin_cap = (size_t)w * h * bpp;
    }
    if (layout == 3 || layout == 2) {
        deswizzle(lin, w * bpp, GP(a), w * bpp, h, bhl);
    } else {
        for (u32 y = 0; y < h; y++) memcpy(lin + (size_t)y * w * bpp, GP(a + (u64)y * pitch), w * bpp);
    }
    if (bpp == 4) {
        if (fmt == 5) {
            for (size_t i = 0; i < (size_t)w * h; i++) {
                u32 p = ((u32*)lin)[i];
                out[i] = (p & 0xFF00FF00u) | ((p >> 16) & 0xFF) | ((p & 0xFF) << 16);
            }
        } else
            memcpy(out, lin, (size_t)w * h * 4);
    } else {
        u16* s = (u16*)lin;
        for (size_t i = 0; i < (size_t)w * h; i++) {
            u32 p = s[i], r, g, b;
            if (fmt == 4) {
                r = (p >> 11) & 31;
                g = (p >> 5) & 63;
                b = p & 31;
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
            } else if (fmt == 6) {
                r = (p >> 11) & 31;
                g = (p >> 6) & 31;
                b = (p >> 1) & 31;
                r = (r << 3) | (r >> 2);
                g = (g << 3) | (g >> 2);
                b = (b << 3) | (b >> 2);
            } else {
                r = ((p >> 12) & 15) * 17;
                g = ((p >> 8) & 15) * 17;
                b = ((p >> 4) & 15) * 17;
            }
            out[i] = r | (g << 8) | (b << 16) | 0xFF000000u;
        }
    }
}
void vi_show_buffer(u32 nvmap_id, u32 offset, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, const i32 crop[4],
                    u32 transform) {
    u64 a = nvmap_addr_by_id(nvmap_id);
    if (!a) {  
        WARN_LIMIT(5, "vi: buffer without memory (nvmap %u)\n", nvmap_id);
        return;
    }

    g_renderer->present_buffer(a + offset, w, h, pitch, layout, bhl, fmt, crop, transform);
    __atomic_add_fetch(&g_frames, 1, __ATOMIC_RELAXED);
    frame_profile_frame();
}

void present_pace(u32 interval) {
    static PMutex lock = PMUTEX_INIT;                         
    static u64 last;                                          
    int fps = __atomic_load_n(&g_opt_fps, __ATOMIC_RELAXED);  
    u64 per, refresh, now, next;
    frame_profile_add(FP_GUEST_INTERVAL, (u64)interval * (1000000000ull / VI_REFRESH_HZ), 1);
    if (!fps || (fps == SNX_FPS_GAME && !interval)) return;
    refresh = fps == SNX_FPS_GAME ? 1000000000ull / VI_REFRESH_HZ : 1000000000ull / (u64)fps;
    per = fps == SNX_FPS_GAME ? refresh * interval : refresh;
    pm_lock(&lock);
    now = plat_ns();
    next = last + per;
    if (!last || now > next + refresh) next = now;  
    last = next;
    pm_unlock(&lock);
    if (next > now) {
        u64 timing = frame_profile_begin();
        plat_sleep_ns((i64)(next - now));
        frame_profile_end(FP_PACING, timing);
    }
}
