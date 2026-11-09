/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/gpu/gpu.h"
#include "nx/gpu/maxwell/engines_int.h"
#include "nx/gpu/renderer.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/memory/mem.h"
#include "nx/runtime/clock.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


u8* g_mem;
u64 g_draws;
int g_renderer_up = 1;
static int draws, clears;
static void count_clear(GpuChannel* ch, u32 v) {
    (void)ch;
    (void)v;
    clears++;
}
static void count_draw(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 base_vertex,
                       u32 base_instance, u32 instances) {
    (void)ch, (void)indexed, (void)topology, (void)first, (void)count, (void)base_vertex, (void)base_instance;
    (void)instances;
    draws++;
}
static int sample_marks, sample_resets;
static u64 sample_address;
static void reset_samples(u32 channel) { (void)channel; sample_resets++; }
static void mark_samples(u32 channel, u64 address) {
    (void)channel;
    sample_marks++;
    sample_address = address;
}
static const Renderer counting = {.name = "counting", .clear = count_clear, .draw = count_draw,
    .occlusion_reset = reset_samples, .occlusion_mark = mark_samples};
const Renderer* g_renderer = &counting;

#define VA 0x100000ull
static u8 vram[64];
int gmem_read(u64 gva, void* dst, u64 size) {
    if (gva < VA || gva - VA + size > sizeof vram) {
        memset(dst, 0, size);
        return -1;
    }
    memcpy(dst, vram + (gva - VA), size);
    return 0;
}
int gmem_write(u64 gva, const void* src, u64 size) {
    if (gva < VA || gva - VA + size > sizeof vram) return -1;
    memcpy(vram + (gva - VA), src, size);
    return 0;
}
u32 gmem_r32(u64 gva) {
    u32 v = 0;
    gmem_read(gva, &v, 4);
    return v;
}
void gmem_w32(u64 gva, u32 v) { gmem_write(gva, &v, 4); }
u64 gmmu_translate(u64 gva) { return gva; }
u64 gmmu_run(u64 gva, u64 max, u64* cpu) { *cpu = gva; return gva ? max : 0; }
u32 gpu_color_bpp(u32 format) {
    (void)format;
    return 4;
}
u64 bl_offset(u32 x_bytes, u32 y, u32 z, u32 width_bytes, u32 height, u32 bh_log2, u32 bd_log2) {
    (void)x_bytes, (void)y, (void)z, (void)width_bytes, (void)height, (void)bh_log2, (void)bd_log2;
    return 0;
}
void gpu_signal_point(void) {}
void gpu_release_write(u64 gva, const void* data, u32 len) { gmem_write(gva, data, len); }
u64 guest_ns(void) { return 0; }
static struct {  
    u32 kind, val;
    u64 addr;
    int geq, n;
} parked;
void gpu_park(GpuChannel* ch, u32 kind, u64 addr, u32 val, int geq) {
    (void)ch;
    parked.kind = kind, parked.addr = addr, parked.val = val, parked.geq = geq;
    parked.n++;
}
void pm_lock(PMutex* m) { (void)m; }
void pm_unlock(PMutex* m) { (void)m; }
void i2m_launch(GpuChannel* ch, u32* regs, int base) { (void)ch, (void)regs, (void)base; }
void i2m_data(GpuChannel* ch, u32 v) { (void)ch, (void)v; }
void mme_run(GpuChannel* ch, u32 idx, u32 np) { (void)ch, (void)idx, (void)np; }
void logf_(int lvl, const char* fmt, ...) { (void)lvl, (void)fmt; }


static GpuChannel ch;
static int failures;

static void put64(u32 off, u64 v) { memcpy(vram + off, &v, 8); }
static void render_enable(u64 addr, u32 mode) {
    maxwell3d_method(&ch, 0x554, (u32)(addr >> 32), 0);
    maxwell3d_method(&ch, 0x555, (u32)addr, 0);
    maxwell3d_method(&ch, 0x556, mode, 1);
}

static void report(u64 addr, u32 sel) {
    maxwell3d_method(&ch, 0x6C0, (u32)(addr >> 32), 0);
    maxwell3d_method(&ch, 0x6C1, (u32)addr, 0);
    maxwell3d_method(&ch, 0x6C3, 2u | sel << 23, 1);
}
static void release(u64 addr, u32 seq) {
    maxwell3d_method(&ch, 0x6C0, (u32)(addr >> 32), 0);
    maxwell3d_method(&ch, 0x6C1, (u32)addr, 0);
    maxwell3d_method(&ch, 0x6C2, seq, 0);
    maxwell3d_method(&ch, 0x6C3, 0, 1);
}

static int drawn(void) {
    int d = draws, c = clears;
    r3d_draw(&ch, 0, 4, 0, 3, 0, 0, 1);
    maxwell3d_method(&ch, 0x674, 0x3D, 1);
    d = draws - d;
    c = clears - c;
    return d == 1 && c == 1 ? 1 : d == 0 && c == 0 ? 0 : -1;
}
static void expect(int want, const char* what) {
    int got = drawn();
    if (got != want) {
        fprintf(stderr, "FAIL: %s: %s\n", what,
                got < 0 ? "the draw and the clear disagree"
                : got   ? "drawn"
                        : "skipped");
        failures++;
    }
}

int main(void) {
    expect(1, "a fresh channel draws");
    render_enable(0, 0);
    expect(0, "FALSE skips");
    render_enable(0, 1);
    expect(1, "TRUE draws");


    put64(0, 9600), put64(16, 9600);
    render_enable(VA, 3);
    expect(1, "IF_EQUAL draws over equal values");
    render_enable(VA, 4);
    expect(0, "IF_NOT_EQUAL skips equal values");
    put64(16, 9601);
    render_enable(VA, 3);
    expect(0, "IF_EQUAL skips values that differ in the low word");
    render_enable(VA, 4);
    expect(1, "IF_NOT_EQUAL draws over values that differ in the low word");
    put64(16, 9600 | 1ull << 32);
    render_enable(VA, 3);
    expect(0, "IF_EQUAL skips values that differ only in the high word");
    render_enable(VA, 4);
    expect(1, "IF_NOT_EQUAL draws over values that differ only in the high word");
    put64(8, 1234), put64(24, 5678);
    put64(16, 9600);
    render_enable(VA, 3);
    expect(1, "IF_EQUAL ignores the timestamps");


    put64(16, 1);
    expect(1, "the decision stays until the mode is written again");
    render_enable(VA, 3);
    expect(0, "rewriting the mode reads the memory again");


    put64(0, 1ull | 1ull << 32);
    render_enable(VA, 2);
    expect(1, "CONDITIONAL draws when both words are set");
    put64(0, 1);
    render_enable(VA, 2);
    expect(0, "CONDITIONAL skips when one word is 0");


    report(VA, 0x15);
    report(VA + 16, 0x15);
    render_enable(VA, 4);
    expect(1, "IF_NOT_EQUAL over two made-up samples-passed counts draws");
    render_enable(VA, 3);
    expect(1, "IF_EQUAL over two made-up counts draws");
    release(VA, 7);
    release(VA + 16, 8);
    render_enable(VA, 3);
    expect(0, "IF_EQUAL over two released values that differ skips");
    report(VA, 0);
    report(VA + 16, 0);
    render_enable(VA, 4);
    expect(0, "the ZERO counter is a real 0: IF_NOT_EQUAL over two of them skips");


    render_enable(0, 0);
    maxwell3d_method(&ch, 0x651, 1, 1);
    expect(1, "override ALWAYS draws over FALSE");
    render_enable(0, 1);
    maxwell3d_method(&ch, 0x651, 2, 1);
    expect(0, "override NEVER skips over TRUE");
    maxwell3d_method(&ch, 0x651, 0, 1);
    expect(1, "override USE goes back to TRUE");


    maxwell3d_method(&ch, 0x6C0, 0x12, 0);
    maxwell3d_method(&ch, 0x6C1, 0x345600, 0);
    maxwell3d_method(&ch, 0x6C2, 77, 0);
    maxwell3d_method(&ch, 0x6C3, 1u | 1u << 16, 1);
    if (parked.n != 1 || parked.kind != GPU_PARK_REPORT || parked.addr != 0x1200345600ull || parked.val != 77 ||
        !parked.geq) {
        fprintf(stderr, "FAIL: a report semaphore acquire (GE) asks the stream to wait for its address and value\n");
        failures++;
    }
    maxwell3d_method(&ch, 0x6C3, 1, 1);
    if (parked.n != 2 || parked.geq) {
        fprintf(stderr, "FAIL: a report semaphore acquire (EQ) asks for an equal value\n");
        failures++;
    }

    g_draws = 9;
    maxwell3d_method(&ch, 0x54C, 1, 1);  
    report(VA, 0x15);
    u64 samples = 1;
    memcpy(&samples, vram, sizeof samples);
    if (samples != 0) {
        fprintf(stderr, "FAIL: counter report after reset is %llu, expected zero\n", (unsigned long long)samples);
        failures++;
    }
    g_draws = 10;
    report(VA, 0x15);
    memcpy(&samples, vram, sizeof samples);
    if (!samples) {
        fprintf(stderr, "FAIL: counter report stayed zero after another draw\n");
        failures++;
    }
    static const struct {
        u32 reset, report;
    } reset_pairs[] = {
        {0x01, 0x15}, {0x02, 0x0A}, {0x10, 0x0B}, {0x12, 0x01}, {0x13, 0x03}, {0x13, 0x12}, {0x15, 0x05}, {0x16, 0x1B},
        {0x17, 0x1D}, {0x18, 0x1F}, {0x1A, 0x07}, {0x1B, 0x09}, {0x1C, 0x0F}, {0x1D, 0x11}, {0x1E, 0x13}, {0x1F, 0x12},
    };
    for (u32 i = 0; i < sizeof reset_pairs / sizeof reset_pairs[0]; i++) {
        g_draws = 12;
        maxwell3d_method(&ch, 0x54C, reset_pairs[i].reset, 1);
        report(VA, reset_pairs[i].report);
        memcpy(&samples, vram, sizeof samples);
        if (samples) {
            fprintf(stderr, "FAIL: reset selector %x for report %x\n", reset_pairs[i].reset, reset_pairs[i].report);
            failures++;
        }
    }

    if (failures) return 1;

    g_gpu_force_visible = 1;
    maxwell3d_method(&ch, 0x54C, 1, 1);
    report(VA, 0x15);
    memcpy(&samples, vram, sizeof samples);
    if (!samples) {
        fprintf(stderr, "FAIL: force-visible samples are zero after reset\n");
        failures++;
    }
    report(VA, 0);
    memcpy(&samples, vram, sizeof samples);
    if (samples) {
        fprintf(stderr, "FAIL: force-visible changed the ZERO counter\n");
        failures++;
    }
    release(VA, 0);
    release(VA + 16, 1);
    render_enable(VA, 2);
    expect(1, "force-visible bypasses a false conditional");
    render_enable(VA, 3);
    expect(1, "force-visible bypasses unequal values in IF_EQUAL");
    render_enable(0, 0);
    expect(0, "force-visible preserves explicit FALSE");
    maxwell3d_method(&ch, 0x651, 2, 1);
    render_enable(VA, 2);
    expect(0, "force-visible preserves explicit NEVER");
    maxwell3d_method(&ch, 0x651, 0, 1);
    g_gpu_force_visible = 0;
    release(VA, 0);  
    release(VA + 16, 1);
    render_enable(VA, 2);
    expect(0, "restoring occlusion restores the false conditional");
    if (failures) return 1;
    g_gpu_force_visible = 2;
    maxwell3d_method(&ch, 0x54C, 1, 1);
    if (sample_resets != 1) failures++;
    report(VA, 0x15);
    memcpy(&samples, vram, sizeof samples);
    if (!samples) {
        fprintf(stderr, "FAIL: an unavailable query hid geometry\n");
        failures++;
    }
    if (sample_marks != 1 || sample_address != VA) failures++;
    g_gpu_force_visible = 0;
    if (failures) return 1;
    puts("SetRenderEnable: FALSE, TRUE, CONDITIONAL, IF_EQUAL, IF_NOT_EQUAL and the override gate draws and clears; a "
         "report semaphore acquire waits");
    return 0;
}
