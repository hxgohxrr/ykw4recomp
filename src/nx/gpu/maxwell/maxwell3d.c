/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include <stdlib.h>
#include <string.h>
#include "nx/gpu/maxwell/write_pages.h"
#include "nx/gpu/renderer.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include "nx/gpu/gpu.h"
void r3d_clear(GpuChannel* ch, u32 v) {
    if (ch->render_off) return;  
    if (g_renderer_up) {
        g_renderer->clear(ch, v);
        return;
    }
    u32* r = ch->r3d;
    u32 rt = (v >> 6) & 15;
    if (!(v & 0x3C)) return;
    u32* t = &r[0x200 + rt * 0x10];
    u64 addr = ((u64)t[0] << 32) | t[1];
    u32 w = t[2], h = t[3], fmt = t[4], tm = t[5];
    if (!addr || !w || !h) return;
    float cc[4];
    memcpy(cc, &r[0x360], 16);
    u32 bpp = gpu_color_bpp(fmt);
    u8 px[16];
    if (bpp == 4) {
        u32 c = 0;
        for (int i = 0; i < 4; i++) {
            float f = cc[i] < 0 ? 0 : cc[i] > 1 ? 1 : cc[i];
            c |= (u32)(f * 255.f + 0.5f) << (i * 8);
        }
        if (fmt == 0xCF || fmt == 0xD0) c = (c & 0xFF00FF00u) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
        memcpy(px, &c, 4);
    } else
        memset(px, 0, 16);
    int linear = (tm >> 12) & 1;
    u32 bh = (tm >> 4) & 15;

    u32 pitch = linear ? w : w * bpp, wb = linear ? w / bpp * bpp : w * bpp;
    if (!wb) return;
    u8* row = malloc(wb);
    if (!row) {
        WARN("gpu: no memory to clear a %ux%u target\n", w, h);
        return;
    }
    for (u32 x = 0; x < wb; x += bpp) memcpy(row + x, px, bpp);
    if (linear) {
        for (u32 y = 0; y < h; y++) gmem_write(addr + (u64)y * pitch, row, wb);
    } else {  
        u64 hi = 0;
        for (u32 y = 0; y < h; y++)
            for (u32 x = 0; x < wb; x += 16) {
                u64 o = bl_offset(x, y, 0, wb, h, bh, 0), cpu = gmmu_translate(addr + o);
                u32 n = wb - x < 16 ? wb - x : 16;
                if (cpu) memcpy(GP(cpu), row + x, n);
                if (o + n > hi) hi = o + n;
            }
        gpu_mem_written(addr, hi);
    }
    free(row);
}
void r3d_draw_texture(GpuChannel* ch) {
    if (g_renderer_up && g_renderer->draw_texture) g_renderer->draw_texture(ch);
}
void r3d_draw(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 base_vertex, u32 base_instance,
              u32 instances) {
    if (ch->render_off) return;  

    if ((ch->r3d[0x652] & 1) && ch->r3d[0x65C]) {

        u32 override = ch->r3d[0x65C] & 0xF;
        topology = override == 1 ? 0 : override == 2 ? 1 : override;
    }
    if (g_renderer_up) {
        g_renderer->draw(ch, indexed, topology, first, count, base_vertex, base_instance, instances);
        return;
    }
    (void)ch;
    (void)indexed;
    (void)topology;
    (void)first;
    (void)count;
    (void)base_vertex;
    (void)base_instance;
    (void)instances;
    g_draws++;
}

void r3d_const_upload(GpuChannel* ch, u32 v, int last) {
    u32* r = ch->r3d;
    if (ch->cb_n == sizeof ch->cb_words / sizeof ch->cb_words[0]) r3d_const_flush(ch);
    if (!ch->cb_n) ch->cb_addr = (((u64)r[0x8E1] << 32) | r[0x8E2]) + r[0x8E3];
    ch->cb_words[ch->cb_n++] = v;
    r[0x8E3] += 4;
    if (last) r3d_const_flush(ch);
}
void r3d_const_flush(GpuChannel* ch) {
    if (!ch->cb_n) return;
    gmem_write(ch->cb_addr, ch->cb_words, (u64)ch->cb_n * 4);
    ch->cb_n = 0;
}

void r3d_cb_bind(GpuChannel* ch, int stage, u32 v) {
    u32* r = ch->r3d;
    u32 idx = (v >> 4) & 31;
    if (stage > 4 || idx >= 18) return;
    ch->cbind[stage][idx] = (v & 1) ? ((u64)r[0x8E1] << 32) | r[0x8E2] : 0;
    ch->cbind_size[stage][idx] = (v & 1) ? r[0x8E0] : 0;
}
int r3d_blit(const Surf2D* s, const Surf2D* d, int dx, int dy, int dw, int dh, double sx, double sy, double sdx,
             double sdy) {
    if (g_renderer_up) return g_renderer->blit(s, d, dx, dy, dw, dh, sx, sy, sdx, sdy);
    return 0;
}


static u64 g_gw_gen;       
static u64 g_gw_phys_gen;  
static PMutex g_gwlock = PMUTEX_INIT;
static WritePages g_gw_va, g_gw_phys;  
static void mem_written(u64 gva, u64 size) {
    if (!size) return;
    pm_lock(&g_gwlock);
    u64 gen = g_gw_gen + 1;
    write_pages_stamp(&g_gw_va, gva, size, gen);
    for (u64 a = gva, left = size; left;) {
        u64 cpu = 0, n = gmmu_run(a, left, &cpu);
        if (n) write_pages_stamp(&g_gw_phys, cpu, n, gen);
        else { n = 0x1000 - (a & 0xFFF); if (n > left) n = left; }
        a += n;
        left -= n;
    }
    __atomic_store_n(&g_gw_phys_gen, gen, __ATOMIC_RELEASE);
    __atomic_store_n(&g_gw_gen, gen, __ATOMIC_RELEASE);
    pm_unlock(&g_gwlock);
}
void gpu_mem_written(u64 gva, u64 size) { mem_written(gva, size); }
void gpu_mem_written_shader(u64 gva, u64 size) { mem_written(gva, size); }
u64 gpu_mem_generation(void) { return __atomic_load_n(&g_gw_gen, __ATOMIC_ACQUIRE); }
static int phys_dirty_since(u64 since, u64 a, u64 sz) {
    if (!sz || since >= __atomic_load_n(&g_gw_phys_gen, __ATOMIC_ACQUIRE)) return 0;
    for (u64 left = sz; left;) {
        u64 cpu = 0, n = gmmu_run(a, left, &cpu);
        if (n) {
            if (write_pages_since(&g_gw_phys, since, cpu, n)) return 1;
        } else { n = 0x1000 - (a & 0xFFF); if (n > left) n = left; }
        a += n;
        left -= n;
    }
    return 0;
}

static int dirty_since_locked(u64 since, u64 a, u64 sz) {
    if (!sz || since >= __atomic_load_n(&g_gw_gen, __ATOMIC_ACQUIRE)) return 0;
    return write_pages_since(&g_gw_va, since, a, sz) || phys_dirty_since(since, a, sz);
}
int gpu_mem_dirty_since(u64 since, u64 a, u64 sz) {

    static int lockfree = -1;
    int enabled = __atomic_load_n(&lockfree, __ATOMIC_ACQUIRE);
    if (enabled < 0) {
        const char* value = getenv("SNX_GPU_WRITE_LOCKFREE");
        enabled = value && strcmp(value, "0") != 0;
        __atomic_store_n(&lockfree, enabled, __ATOMIC_RELEASE);
    }
    if (enabled) return dirty_since_locked(since, a, sz);
    pm_lock(&g_gwlock);
    int dirty = dirty_since_locked(since, a, sz);
    pm_unlock(&g_gwlock);
    return dirty;
}
