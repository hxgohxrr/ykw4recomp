/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/runtime/arm.h"
#include "nx/runtime/clock.h"
#include "nx/runtime/loader.h"
#include "nx/runtime/main.h"
#include "nx/runtime/rtapi.h"
#include "nx/hle/kernel/kernel_int.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include "common/cpu/arm/exec.h"
#include <stdlib.h>
#include <string.h>

const SnxArmModule* g_arm_mod;
static SnxModule g_view;


static u32 g_lo, g_hi;
static SnxArmBlock *g_a32, *g_t32;

static PMutex g_jit_lock;
static SnxArmExec* g_jit;
typedef struct {
    u32 key;  
    SnxArmBlock fn;
} Extra;
static Extra* g_extra;
static int g_nextra, g_capextra;

const SnxModule* arm_module(const SnxArmModule* m) {
    if (!m || m->abi != SNX_ARM_MODULE_ABI) return NULL;
    g_arm_mod = m;
    g_view.abi = SNX_ABI;
    g_view.nmods = m->nmods;
    g_view.mods = m->mods;
    g_view.title = m->title;
    g_view.title_id = m->title_id;
    g_view.main_stack = m->main_stack;
    g_view.main_prio = m->main_prio;
    g_view.main_core = m->main_core;
    return &g_view;
}

void arm_dispatch_init(void) {
    const SnxArmModule* m = g_arm_mod;
    u64 lo = ~0ull, hi = 0;
    pm_init(&g_jit_lock);
    for (u32 i = 0; i < m->nmods; i++) {
        u64 a = m->mods[i].base + m->mods[i].text_off;
        if (a < lo) lo = a;
        if (a + m->mods[i].text_size > hi) hi = a + m->mods[i].text_size;
    }
    if (lo >= hi || hi > 0x100000000ull) lo = hi = 0;
    g_lo = (u32)lo;
    g_hi = (u32)hi;
    g_a32 = calloc((g_hi - g_lo) / 4 + 1, sizeof *g_a32);
    g_t32 = calloc((g_hi - g_lo) / 2 + 1, sizeof *g_t32);
    if (!g_a32 || !g_t32) fatal("Out of memory for the table of the game's code");
    for (u32 i = 0; i < m->nentries; i++) {
        u32 key = m->entry_addr[i], a = key & ~1u;
        if (a < g_lo || a >= g_hi) continue;
        if (key & 1)
            g_t32[(a - g_lo) / 2] = m->entry_fn[i];
        else
            g_a32[(a - g_lo) / 4] = m->entry_fn[i];
    }
    LOG("AArch32: %u recompiled blocks in %x-%x\n", m->nentries, g_lo, g_hi);
}

static SnxArmBlock* slot(u32 pc, int thumb) {
    if (pc < g_lo || pc >= g_hi) return NULL;
    return thumb ? &g_t32[(pc - g_lo) / 2] : &g_a32[(pc - g_lo) / 4];
}


static SnxArmBlock compile(u32 pc, int thumb) {
    SnxArmBlock *s = slot(pc, thumb), b = NULL;
    u32 key = pc | (thumb ? 1u : 0u);
    pm_lock(&g_jit_lock);
    if (s) b = *s;  
    for (int i = 0; !s && !b && i < g_nextra; i++)
        if (g_extra[i].key == key) b = g_extra[i].fn;
    if (!b) {
        MemBlock m;
        char error[256];
        mem_query(pc, &m);
        if (!g_jit) {
            g_jit = snx_arm_exec_new(error, sizeof error);
            if (!g_jit) fatal("Cannot compile the game's code while it runs: %s", error);
        }
        SnxArmExecRange range = {m.addr, NULL, (size_t)m.size};
        if ((m.perm & PERM_X) && m.state != MS_FREE) b = snx_arm_exec_block(g_jit, &range, 1, pc, thumb, g_mem);
        if (b && s)
            __atomic_store_n(s, b, __ATOMIC_RELEASE);
        else if (b) {
            if (g_nextra == g_capextra) {
                g_capextra = g_capextra ? g_capextra * 2 : 64;
                g_extra = realloc(g_extra, g_capextra * sizeof *g_extra);
                if (!g_extra) fatal("Out of memory for the game's code");
            }
            g_extra[g_nextra].key = key;
            g_extra[g_nextra++].fn = b;
        }
        if (b) {
            LOGV("AArch32: compiled %s block %x while running (not recompiled)\n", thumb ? "Thumb" : "A32", pc);
            g_rtapi.note_hint(key);  
        }
    }
    pm_unlock(&g_jit_lock);
    return b;
}

static SnxArmBlock lookup(u32 pc, int thumb) {
    SnxArmBlock* s = slot(pc, thumb);
    SnxArmBlock b = s ? __atomic_load_n(s, __ATOMIC_ACQUIRE) : NULL;
    return b ? b : compile(pc, thumb);
}

static const char* exit_name(u32 e) {
    switch (e) {
        case SNX_ARM_UNDEFINED: return "an undefined instruction";
        case SNX_ARM_BREAKPOINT: return "BKPT";
        case SNX_ARM_DATA_ABORT: return "an access outside its address space";
        case SNX_ARM_ALIGNMENT: return "a misaligned access";
        case SNX_ARM_BAD_ENTRY: return "a block entered in the wrong state";
        case SNX_ARM_ENDIAN_UNSUPPORTED: return "big-endian data (CPSR.E)";
        case SNX_ARM_BACKEND_UNSUPPORTED: return "an instruction the block compiler can't run";
        default: return "an exit the runtime doesn't know";
    }
}

static void stop(KThread* t, const char* what, u32 pc, u32 detail) {
    WARN("\n*** The game's thread %llu stopped at %x: %s (%x) ***\n", (unsigned long long)t->id, pc, what, detail);
    kernel_dump_regs(&t->cpu);
    save_hints();
    fatal("The game stopped at %x: %s. See the .log for details.", pc, what);
}

void arm_thread_run(KThread* t) {
    SnxArmCPU* c = &t->arm;
    LOGV("[thread %llu] starts at %x%s\n", (unsigned long long)t->id, c->r[15], c->cpsr & SNX_ARM_T ? " (Thumb)" : "");
    for (;;) {
        u32 pc = c->r[15];
        int thumb = (c->cpsr & SNX_ARM_T) != 0;
        if (UNLIKELY(!pc)) svc_exit_thread(t);  
        if (UNLIKELY(!thumb && (pc & 3))) stop(t, "a branch to a misaligned A32 address", pc, pc);
        SnxArmBlock b = lookup(pc, thumb);
        if (UNLIKELY(!b)) stop(t, "a branch to an address with no code", pc, pc);
        b(c);
        if (UNLIKELY(c->exit)) {
            u32 e = c->exit;
            c->exit = SNX_ARM_OK;
            if (e == SNX_ARM_SVC)
                snx_svc32(c, c->syndrome);
            else
                stop(t, exit_name(e), c->r[15], c->syndrome);
        }

        if (UNLIKELY(__atomic_load_n(&t->cpu.poll, __ATOMIC_RELAXED))) core_poll(t);
    }
}

u32 arm_block_at(const void* host_pc, u64* offset) {
    u64 best = 0, p = (u64)(size_t)host_pc;
    u32 key = 0;
    if (!g_arm_mod) return 0;
    for (u32 i = 0; i < g_arm_mod->nentries; i++) {
        u64 fn = (u64)(size_t)g_arm_mod->entry_fn[i];
        if (fn <= p && fn > best) {
            best = fn;
            key = g_arm_mod->entry_addr[i];
        }
    }
    if (!best || p - best >= 0x10000) return 0;
    *offset = p - best;
    return key;
}




static void rt_undefined(SnxArmCPU* c, u32 word, u32 reason) {
    KThread* t = kthread_cur();
    WARN("\nUnsupported %s instruction %08x at %x%s\n", c->cpsr & SNX_ARM_T ? "Thumb" : "A32", word, c->r[15],
         reason == SNX_ARM_BACKEND_UNSUPPORTED ? " (the block compiler can't run it)" : "");
    if (t) kernel_dump_regs(&t->cpu);
    save_hints();
    fatal("Unsupported instruction %08x at %x", word, c->r[15]);
}


static void rt_coproc(SnxArmCPU* c, u32 w) {
    unsigned rt = (w >> 12) & 15, rt2 = (w >> 16) & 15;
    u32 r = w & 0x0FFF0FFF;
    if ((w & 0x0FF00F0F) == 0x0C500F0E && ((w >> 4) & 15) <= 1 && rt != 15 && rt2 != 15 && rt != rt2) {
        u64 ticks = guest_ticks();  
        c->r[rt] = (u32)ticks;
        c->r[rt2] = (u32)(ticks >> 32);
        return;
    }
    if ((w & 0x0FFF0FFF) == 0x0E1E0F10 && rt != 15) {  
        c->r[rt] = 19200000;
        return;
    }
    if (r == 0x0E070FBA || r == 0x0E070F9A || r == 0x0E070F95) {  
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        return;
    }
    snx_arm_coproc_user(c, w);
}

const SnxArmRt g_arm_rt = {rt_undefined, rt_coproc};
