/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/runtime/rtapi.h"
#include "nx/runtime/clock.h"
#include "nx/runtime/main.h"
#include "nx/hle/kernel/kernel.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_hints_path[1200];
static PMutex g_hint_lock;
static u64* g_hints;  
static int g_nhints, g_caphints, g_hints_dirty;

static u64* g_hset;
static u32 g_hset_cap;         
static int hset_add(u64 pc) {  
    if ((u64)(g_nhints + 1) * 2 > g_hset_cap) {
        u32 cap = g_hset_cap ? g_hset_cap * 2 : 1024;
        u64* n = calloc(cap, 8);
        for (u32 i = 0; i < g_hset_cap; i++)
            if (g_hset[i]) {
                u32 j = (u32)((g_hset[i] >> 2) * 0x9E3779B97F4A7C15ull >> 32) & (cap - 1);
                while (n[j]) j = (j + 1) & (cap - 1);
                n[j] = g_hset[i];
            }
        free(g_hset);
        g_hset = n;
        g_hset_cap = cap;
    }
    u32 j = (u32)((pc >> 2) * 0x9E3779B97F4A7C15ull >> 32) & (g_hset_cap - 1);
    while (g_hset[j]) {
        if (g_hset[j] == pc) return 0;
        j = (j + 1) & (g_hset_cap - 1);
    }
    g_hset[j] = pc;
    if (g_nhints == g_caphints) {
        g_caphints = g_caphints ? g_caphints * 2 : 256;
        g_hints = realloc(g_hints, 8 * g_caphints);
    }
    g_hints[g_nhints++] = pc;
    return 1;
}
static void rt_note_hint(u64 pc) {
    if (!pc) return;
    pm_lock(&g_hint_lock);
    if (hset_add(pc)) {
        g_hints_dirty = 1;
        LOGV("interpreter: entering %llx (not recompiled)\n", (unsigned long long)pc);
    }
    pm_unlock(&g_hint_lock);
}

void hints_init(void) {
    snprintf(g_hints_path, sizeof g_hints_path, "%s%s.hints", g_exedir, g_exename);
    pm_init(&g_hint_lock);
}

void save_hints(void) {
    pm_lock(&g_hint_lock);
    FILE* f = g_hints_dirty ? fopen(g_hints_path, "w") : NULL;
    if (f) {
        for (int i = 0; i < g_nhints; i++) fprintf(f, "%llx\n", (unsigned long long)g_hints[i]);
        fclose(f);
        g_hints_dirty = 0;
    }
    pm_unlock(&g_hint_lock);
}
void load_hints(void) {
    FILE* f = fopen(g_hints_path, "r");
    if (!f) return;
    char ln[64];
    while (fgets(ln, sizeof ln, f)) {
        u64 v = strtoull(ln, NULL, 16);
        if (v) hset_add(v);
    }
    fclose(f);
}

static void rt_undef(CPU* c, u64 pc, u32 w) {
    WARN("\nUnsupported instruction %08x at %llx\n", w, (unsigned long long)pc);
    c->pc = pc;
    kernel_dump_regs(c);
    save_hints();
    fatal("Unsupported instruction %08x at %llx", w, (unsigned long long)pc);
}
static void rt_brk(CPU* c, u64 pc, u32 imm) {
    WARN("\nBRK #%u at %llx\n", imm, (unsigned long long)pc);
    c->pc = pc;
    kernel_dump_regs(c);
    save_hints();
    fatal("The game executed BRK at %llx", (unsigned long long)pc);
}
static u64 rt_ticks(void) { return guest_ticks(); }
void snx_fpcr_apply(CPU* c) {

    (void)c;
}
static void rt_fpcr(CPU* c) { snx_fpcr_apply(c); }
static void rt_sev(void) {}
static void rt_wfe(CPU* c) {  
    (void)c;
    KThread* t = kthread_cur();
    core_leave(t);
    plat_yield();
    if (t) core_enter(t);
}
static void rt_poll(CPU* c) { core_poll((KThread*)c->thread); }
const RtAPI g_rtapi = {snx_svc, rt_undef, rt_note_hint, rt_ticks, rt_brk, rt_fpcr, rt_sev, rt_wfe, rt_poll};
