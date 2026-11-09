/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/hle/kernel/kernel_int.h"
#include "nx/runtime/arm.h"
static void snx_svc_(CPU* c, u32 imm);

typedef struct {
    KObj o;
    KEvent* ev;
} KWEvent;
static void wev_destroy(KObj* o) {
    kobj_unref(&((KWEvent*)o)->ev->o);
    free(o->waiters);
    free(o);
}


static void park_nl(KThread* t, int at) {
    if (!t->suspended) return;
    t->pause_at = at;
    for (int i = 0; i < t->o.nwait; i++)  
        if (t->o.waiters[i]->wait_kind == 5) wake(t->o.waiters[i], 0);
    core_leave(t);  
    while (t->suspended) pc_wait(&t->cv, &g_klock, -1);
    t->pause_at = 0;
}
static void svc_enter(KThread* t) {
    __atomic_store_n(&t->in_svc, 1, __ATOMIC_SEQ_CST);
    if (UNLIKELY(__atomic_load_n(&t->suspended, __ATOMIC_SEQ_CST))) {
        pm_lock(&g_klock);
        park_nl(t, 1);
        pm_unlock(&g_klock);
    }
}

static void svc_leave(KThread* t) {
    for (;;) {
        core_enter(t);
        core_preempt(t);
        __atomic_store_n(&t->in_svc, 0, __ATOMIC_SEQ_CST);
        if (LIKELY(!__atomic_load_n(&t->suspended, __ATOMIC_SEQ_CST))) return;
        pm_lock(&g_klock);
        __atomic_store_n(&t->in_svc, 1, __ATOMIC_SEQ_CST);
        park_nl(t, 2);
        pm_unlock(&g_klock);
    }
}

static void wait_stopped_nl(KThread* self, KThread* n) {
    while (n->suspended && n->state != 2 && !__atomic_load_n(&n->in_svc, __ATOMIC_SEQ_CST)) {
        self->woken = 0;
        self->wait_kind = 5;
        self->wait_name = "a thread to pause";
        self->wait_timeout = -1;
        waiter_add(&n->o, self);
        sleep_nl(self, -1);
        waiter_del(&n->o, self);
        self->wait_kind = 0;
    }
}


static void thread_context32_nl(const KThread* n, u8* p) {
    const SnxArmCPU* a = &n->arm;
    int inside = n->pause_at == 0;
    u64 pc = n->pause_at == 2 ? a->r[15] : a->r[15] - (a->cpsr & SNX_ARM_T ? 2 : 4);  
    u64 sp = a->r[13], lr = a->r[14];
    u32 fpsr = a->fpscr & 0xF800009Fu, fpcr = a->fpscr & 0x07FF9F00u;
    memset(p, 0, 0x320);
    if (n->state == 2) return;
    for (int i = inside ? 4 : 0; i < (inside ? 12 : 15); i++) {
        u64 r = a->r[i];
        memcpy(p + i * 8, &r, 8);
    }
    memcpy(p + 13 * 8, &sp, 8);
    memcpy(p + 14 * 8, &lr, 8);
    memcpy(p + 0xF0, &lr, 8);
    memcpy(p + 0xF8, &sp, 8);
    memcpy(p + 0x100, &pc, 8);
    memcpy(p + 0x108, &a->cpsr, 4);
    if (inside)
        memcpy(p + 0x110 + 8 * 8, &a->d[8], 8 * 8);
    else
        memcpy(p + 0x110, a->d, 32 * 8);
    memcpy(p + 0x310, &fpcr, 4);
    memcpy(p + 0x314, &fpsr, 4);
    {
        u64 tls = a->tpidrurw;
        memcpy(p + 0x318, &tls, 8);
    }
}
static void thread_context_nl(const KThread* n, u8* p) {
    if (g_arm_mod) {
        thread_context32_nl(n, p);
        return;
    }
    const CPU* c = &n->cpu;
    int inside = n->pause_at == 0;
    u64 pc = n->pause_at == 2 ? c->pc : c->pc - 4;  
    u32 pstate = get_nzcv(c);
    memset(p, 0, 0x320);
    if (n->state == 2) return;  
    if (inside)
        memcpy(p + 19 * 8, &c->x[19], 10 * 8);
    else
        memcpy(p, c->x, 29 * 8);
    memcpy(p + 0xE8, &c->x[29], 3 * 8);  
    memcpy(p + 0x100, &pc, 8);
    memcpy(p + 0x108, &pstate, 4);
    if (inside)
        memcpy(p + 0x110 + 8 * 16, &c->v[8], 8 * 16);
    else
        memcpy(p + 0x110, c->v, 32 * 16);
    memcpy(p + 0x310, &c->fpcr, 4);
    memcpy(p + 0x314, &c->fpsr, 4);
    memcpy(p + 0x318, &c->tpidr, 8);
}

void snx_svc(CPU* c, u32 imm) {
    u64 a0 = c->x[0], a1 = c->x[1], a2 = c->x[2];
    u64 a3 = c->x[3];
    KThread* t = kthread_cur();
    svc_enter(t);
    snx_svc_(c, imm);
    if (imm != 0x1E && imm != 0x0B && imm != 0x10) {
        u32 i = __atomic_fetch_add(&g_svc_ring_n, 1, __ATOMIC_RELAXED) & 63;
        SvcRec* r = &g_svc_ring[i];
        r->imm = imm;
        r->lr = c->x[30];
        r->a[0] = a0;
        r->a[1] = a1;
        r->a[2] = a2;
        r->a[3] = a3;
        r->res = (u32)c->x[0];
        r->o1 = c->x[1];
    }
    static u32 nbad;

    if ((u32)c->x[0] == R_BADHANDLE && imm != 0x16 && imm != 0x1E && imm != 0x0B && imm != 0x10 &&
        (nbad++ < 20 || !(nbad & 0xFFFF)))
        WARN("SVC 0x%x with invalid handle (args %llx %llx %llx) pc=%llx\n", imm, (unsigned long long)a0,
             (unsigned long long)a1, (unsigned long long)a2, (unsigned long long)c->pc);
    svc_leave(t);
}

#define R32_NONE 0xFF
typedef struct {
    u8 x, lo, hi;  
} Reg32;
typedef struct {
    u8 imm, nin, nout;
    Reg32 in[6], out[3];
} Svc32;
static const Svc32 g_svc32[] = {
    {0x08,
     5,
     2,
     {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 3, R32_NONE}, {4, 0, R32_NONE}, {5, 4, R32_NONE}},
     {{0, 0, R32_NONE}, {1, 1, R32_NONE}}},                                             
    {0x0B, 1, 0, {{0, 0, 1}}, {{0}}},                                                   
    {0x0E, 1, 3, {{2, 2, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, R32_NONE}, {2, 2, 3}}},  
    {0x0F, 3, 1, {{0, 0, R32_NONE}, {1, 1, R32_NONE}, {2, 2, 3}}, {{0, 0, R32_NONE}}},  
    {0x18, 3, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 0, 3}}, {{0, 0, R32_NONE}, {1, 1, R32_NONE}}},  
    {0x1C, 4, 1, {{0, 0, R32_NONE}, {1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 3, 4}}, {{0, 0, R32_NONE}}},  
    {0x1E, 0, 1, {{0}}, {{0, 0, 1}}},                                                              
    {0x24, 1, 2, {{1, 1, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, 2}}},                               
    {0x25, 1, 2, {{1, 1, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, 2}}},                               
    {0x29, 3, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 0, 3}}, {{0, 0, R32_NONE}, {1, 1, 2}}},  
    {0x30, 2, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, 2}}},  
    {0x31, 2, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, 2}}},  
    {0x34,
     4,
     1,
     {{0, 0, R32_NONE}, {1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 3, 4}},
     {{0, 0, R32_NONE}}},                                                               
    {0x37, 2, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}}, {{0, 0, R32_NONE}, {1, 1, 2}}},  
    {0x43,
     4,
     2,
     {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 3, R32_NONE}, {4, 0, 4}},
     {{0, 0, R32_NONE}, {1, 1, R32_NONE}}},  
    {0x44,
     6,
     2,
     {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 3, R32_NONE}, {4, 0, R32_NONE}, {5, 4, R32_NONE}, {6, 5, 6}},
     {{0, 0, R32_NONE}, {1, 1, R32_NONE}}},                                      
    {0x5D, 3, 1, {{0, 0, R32_NONE}, {1, 2, 3}, {2, 1, 4}}, {{0, 0, R32_NONE}}},  
    {0x5E, 3, 1, {{0, 0, R32_NONE}, {1, 2, 3}, {2, 1, 4}}, {{0, 0, R32_NONE}}},  
    {0x5F, 3, 1, {{0, 0, R32_NONE}, {1, 2, 3}, {2, 1, 4}}, {{0, 0, R32_NONE}}},  

    {0x6F, 3, 2, {{1, 1, R32_NONE}, {2, 2, R32_NONE}, {3, 0, 3}}, {{0, 0, R32_NONE}, {1, 1, 2}}},
};
void snx_svc32(SnxArmCPU* a, u32 imm) {
    KThread* t = kthread_cur();
    CPU* c = &t->cpu;
    const Svc32* s = NULL;
    for (size_t i = 0; i < sizeof g_svc32 / sizeof *g_svc32; i++)
        if (g_svc32[i].imm == imm) s = &g_svc32[i];
    if (s)
        for (int i = 0; i < s->nin; i++) {
            const Reg32* r = &s->in[i];
            c->x[r->x] = a->r[r->lo] | (r->hi == R32_NONE ? 0 : (u64)a->r[r->hi] << 32);
        }
    else
        for (int i = 0; i < 8; i++) c->x[i] = a->r[i];
    c->x[30] = a->r[14];  
    c->pc = a->r[15];     
    snx_svc(c, imm);
    if (s)
        for (int i = 0; i < s->nout; i++) {
            const Reg32* r = &s->out[i];
            a->r[r->lo] = (u32)c->x[r->x];
            if (r->hi != R32_NONE) a->r[r->hi] = (u32)(c->x[r->x] >> 32);
        }
    else
        for (int i = 0; i < 8; i++) a->r[i] = (u32)c->x[i];
}

static void snx_svc_(CPU* c, u32 imm) {
    KThread* t = kthread_cur();
    u64* x = c->x;
    switch (imm) {
        case 0x01:
            x[0] = svc_set_heap_size(x[1]);
            if (!x[0]) x[1] = HEAP_BASE;
            return;
        case 0x02:
            mem_set_state(x[0], x[1], 0xFFFFFFFFu, (u32)x[2]);
            x[0] = 0;
            return;
        case 0x03:
            mem_set_attr(x[0], x[1], (u32)x[2], (u32)x[3]);
            x[0] = 0;
            return;
        case 0x04: x[0] = svc_map_memory(x[0], x[1], x[2]); return;
        case 0x05: x[0] = svc_unmap_memory(x[0], x[1], x[2]); return;
        case 0x06:
            x[0] = svc_query(x[0], x[2]);
            x[1] = 0;
            return;
        case 0x07: process_exit(0); return;
        case 0x08: {
            int prio = (int)(u32)x[4], core = (int)(i32)(u32)x[5];
            if (core == -2) core = g_default_core;

            if (core < 0 || core >= 64 || !((g_core_mask >> core) & 1)) {
                x[0] = R_BADCORE;
                return;
            }
            if (prio < 0 || prio >= 64 || !((g_prio_mask >> prio) & 1)) {
                x[0] = R_BADPRIO;
                return;
            }
            KThread* n = kthread_create(x[1], x[2], x[3], prio, core);
            u32 h = h_new(&n->o);
            LOGV("CreateThread entry=%llx stack=%llx prio=%d core=%d -> %x\n", (unsigned long long)x[1],
                 (unsigned long long)n->stack_top, prio, core, h);
            x[0] = 0;
            x[1] = h;
            return;
        }
        case 0x09: {
            KThread* n = (KThread*)h_get_type((u32)x[0], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            if (n->state == 0) kthread_start(n);
            x[0] = 0;
            return;
        }
        case 0x0A: svc_exit_thread(t); return;
        case 0x0B: {
            i64 ns = (i64)x[0];
            t->sleep_lr = x[30];
            t->sleep_n++;
            memcpy(t->sleep_x, x, sizeof t->sleep_x);
            if (ns <= 0) {

                core_yield(t, ns ? 2 : 0);  
                if (!(++t->yields & 63)) plat_yield();
            } else {
                core_leave(t);
                plat_sleep_ns(ns);
            }
            return;
        }
        case 0x0C: {
            KThread* n = (KThread*)h_get_type((u32)x[1], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            x[0] = 0;
            x[1] = (u32)n->prio;
            return;
        }
        case 0x0D: {
            KThread* n = (KThread*)h_get_type((u32)x[0], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            u32 prio = (u32)x[1];
            if (prio >= 64 || !((g_prio_mask >> prio) & 1)) {
                x[0] = R_BADPRIO;
                return;
            }
            pm_lock(&g_klock);  
            prio_set_base_nl(n, (int)prio);
            pm_unlock(&g_klock);
            x[0] = 0;
            return;
        }
        case 0x0E: {
            KThread* n = (KThread*)h_get_type((u32)x[2], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            x[0] = 0;
            x[1] = (u32)n->ideal_core;
            x[2] = n->affinity;
            return;
        }
        case 0x0F: {  
            KThread* n = (KThread*)h_get_type((u32)x[0], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            i32 core = (i32)(u32)x[1];
            u64 mask = x[2];

            if (core == -2) {
                core = g_default_core;
                mask = 1ull << core;
            } else if ((mask | g_core_mask) != g_core_mask) {
                x[0] = R_BADCORE;
                return;
            } else if (!mask) {
                x[0] = R_BADCOMBO;
                return;
            } else if (core >= NCORES || core < -3) {
                x[0] = R_BADCORE;
                return;
            }
            pm_lock(&g_klock);  
            if (core == -3) core = n->ideal_core;
            if (core >= 0 && !((mask >> core) & 1)) {
                pm_unlock(&g_klock);
                x[0] = R_BADCOMBO;
                return;
            }
            n->ideal_core = core;
            n->affinity = mask;

            if (core >= 0)
                n->core = core;
            else if (!((mask >> n->core) & 1))
                n->core = 63 - __builtin_clzll(mask);
            pm_unlock(&g_klock);
            if (n == t && t->on_core >= 0 && t->on_core != n->core) core_leave(t);  
            x[0] = 0;
            return;
        }
        case 0x10:  
            x[0] = (u32)(t ? (t->on_core >= 0 ? t->on_core : t->core) : 0);
            return;
        case 0x11: {  
            KWEvent* w = (KWEvent*)h_get_ref((u32)x[0], KO_WEVENT);
            if (!w) {
                x[0] = R_BADHANDLE;
                return;
            }
            kevent_signal(w->ev);
            kobj_unref(&w->o);
            x[0] = 0;
            return;
        }
        case 0x12:    
        case 0x17: {  
            KObj* o = h_get_ref((u32)x[0], 0);
            KEvent* e = !o                                    ? NULL
                        : o->type == KO_EVENT                 ? (KEvent*)o
                        : o->type == KO_WEVENT && imm == 0x12 ? ((KWEvent*)o)->ev
                                                              : NULL;
            if (e) {
                pm_lock(&g_klock);
                int was = e->signaled;
                e->signaled = 0;
                pm_unlock(&g_klock);
                x[0] = (imm == 0x17 && !was) ? R_BADSTATE : 0;
            } else
                x[0] = R_BADHANDLE;
            kobj_unref(o);
            return;
        }
        case 0x13: x[0] = svc_map_shm((u32)x[0], x[1], x[2], (u32)x[3]); return;
        case 0x14: x[0] = svc_unmap_shm((u32)x[0], x[1], x[2]); return;
        case 0x15: {  
            if (x[1] & 0xFFF) {
                x[0] = R_BADADDR;
                return;
            }
            if (!x[2] || (x[2] & 0xFFF)) {
                x[0] = R_BADSIZE;
                return;
            }
            KTmem* m = calloc(1, sizeof *m);
            m->o.type = KO_TMEM;
            m->o.destroy = tmem_destroy;
            m->o.name = "tmem";
            m->addr = x[1];
            m->size = x[2];
            m->perm = (u32)x[3];
            mem_set_attr(m->addr, m->size, 1, 1);
            mem_set_state(m->addr, m->size, 0xFFFFFFFFu, m->perm & PERM_RW);
            x[0] = 0;
            x[1] = h_new(&m->o);
            return;
        }
        case 0x16: x[0] = (u32)h_close((u32)x[0]); return;  
        case 0x18: {
            u32 idx = 0;
            u32 r = svc_wait_sync(t, x[1], (u32)x[2], (i64)x[3], &idx);
            x[0] = r;
            x[1] = idx;
            return;
        }
        case 0x19: {
            KThread* n = (KThread*)h_get_type((u32)x[0], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            pm_lock(&g_klock);
            if (n->wait_kind == 1) wake(n, R_CANCELLED);
            n->cancel = 1;
            pm_unlock(&g_klock);
            x[0] = 0;
            return;
        }
        case 0x1A: x[0] = svc_arbitrate_lock(t, (u32)x[0], x[1], (u32)x[2]); return;
        case 0x1B: x[0] = svc_arbitrate_unlock(x[0]); return;
        case 0x1C: x[0] = svc_wait_cv(t, x[0], x[1], (u32)x[2], (i64)x[3]); return;
        case 0x1D: x[0] = svc_signal_cv(x[0], (i32)(u32)x[1]); return;
        case 0x1E: x[0] = c->rt->ticks(); return;
        case 0x1F: {
            char name[16] = {0};
            memcpy(name, GP(x[1]), 12);
            if (!strcmp(name, "sm:")) {
                SvcObj* o = svc_open("sm:");
                x[0] = 0;
                x[1] = session_new(o);
                return;
            }
            WARN("ConnectToNamedPort(%s) unknown\n", name);
            x[0] = R_NOTFOUND;
            return;
        }
        case 0x21:  
            core_leave(t);
            ipc_handle(t, (u32)x[0], t->tls, 0x100);
            return;
        case 0x22: {  
            u32 r = ipc_user_buffer(x[0], x[1]);
            if (r) {
                x[0] = r;
                return;
            }
            core_leave(t);
            ipc_handle(t, (u32)x[2], x[0], x[1]);
            return;
        }
        case 0x24:
            x[0] = 0;
            x[1] = K_PROCESS_ID;
            return;
        case 0x25: {
            KThread* n = (KThread*)h_get_type((u32)x[1], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            x[0] = 0;
            x[1] = n->id;
            return;
        }
        case 0x26: {
            u32 reason = (u32)x[0];
            if (reason & 0x80000000u) {
                LOGV("svcBreak (notice) %x\n", reason);
                x[0] = 0;
                return;
            }
            WARN("\n*** The game called svcBreak (reason=%x, info=%llx size=%llx) ***\n", reason,
                 (unsigned long long)x[1], (unsigned long long)x[2]);
            if (x[1] && x[2] >= 4 && x[2] < 0x1000) {
                u8 tmp[64];
                memcpy(tmp, GP(x[1]), x[2] < 64 ? x[2] : 64);
                WARN("  data: ");
                for (u64 i = 0; i < (x[2] < 64 ? x[2] : 64); i++) WARN("%02x", tmp[i]);
                WARN("\n");
            }
            kernel_dump_regs(c);

            for (int r = 0; r < 31; r++) {
                u64 a = c->x[r];
                if (a < 0x10000 || a >= AS_SIZE - 256) continue;
                MemBlock b;
                mem_query(a, &b);
                if (b.state == MS_FREE || b.state == MS_INACCESSIBLE) continue;
                u64 room = b.addr + b.size - a;  
                int max = room < 200 ? (int)room : 200;
                const char* p = (const char*)GP(a);
                int n = 0;
                while (n < max && p[n] >= 32 && p[n] < 127) n++;
                if (n >= 4 && (n == max || p[n] == 0)) WARN("  x%d -> \"%.*s\"\n", r, n, p);
            }
            fatal("The game stopped (svcBreak). See the .log for details.");
            return;
        }
        case 0x27: {
            u64 n = x[1];
            if (n > 4096) n = 4096;
            char buf[4100];
            memcpy(buf, GP(x[0]), n);
            buf[n] = 0;
            LOG("[game] %s%s", buf, (n && buf[n - 1] == '\n') ? "" : "\n");
            x[0] = 0;
            return;
        }
        case 0x29: {
            u64 o = 0;
            x[0] = svc_get_info((u32)x[1], (u32)x[2], x[3], &o);
            x[1] = o;
            return;
        }
        case 0x2A:
        case 0x2B:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        case 0x36: x[0] = 0; return;
        case 0x2C: x[0] = svc_map_physical(x[0], x[1]); return;
        case 0x2D: x[0] = svc_unmap_physical(x[0], x[1]); return;
        case 0x2E: x[0] = R_NOTFOUND; return;
        case 0x2F: x[0] = R_NOTFOUND; return;
        case 0x30:
        case 0x37: {
            u32 which = (u32)x[2];
            x[0] = 0;
            x[1] = which == 0 ? mem_total() : which == 1 ? THREAD_MAX : which == 2 ? 800 : which == 3 ? 256 : 64;
            return;
        }
        case 0x31: {
            u32 which = (u32)x[2];
            x[0] = 0;
            x[1] = which == 0 ? used_memory() : which == 1 ? (u64)kthread_live_count() : 16;
            return;
        }
        case 0x32: {  
            u32 act = (u32)x[1];
            if (act > 1) {
                x[0] = R_BADENUM;
                return;
            }
            KThread* n = (KThread*)h_get_type((u32)x[0], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            if (n == t) {
                x[0] = R_BUSY;
                return;
            }
            pm_lock(&g_klock);
            if (n->state != 1 || n->suspended == (int)act)  
                x[0] = R_BADSTATE;
            else {
                __atomic_store_n(&n->suspended, (int)act, __ATOMIC_SEQ_CST);
                if (act)
                    wait_stopped_nl(t, n);
                else
                    pc_signal(&n->cv);  
                x[0] = 0;
            }
            pm_unlock(&g_klock);
            return;
        }
        case 0x33: {  
            KThread* n = (KThread*)h_get_type((u32)x[1], KO_THREAD);
            if (!n) {
                x[0] = R_BADHANDLE;
                return;
            }
            if (n == t) {
                x[0] = R_BUSY;
                return;
            }
            u8 ctx[0x320];
            pm_lock(&g_klock);
            if (n->suspended) wait_stopped_nl(t, n);  
            u32 r = n->suspended ? 0 : R_BADSTATE;
            if (!r) thread_context_nl(n, ctx);
            pm_unlock(&g_klock);
            if (!r) memcpy(GP(x[0]), ctx, sizeof ctx);
            x[0] = r;
            return;
        }
        case 0x34: x[0] = svc_wait_addr(t, x[0], (u32)x[1], (i32)(u32)x[2], (i64)x[3]); return;
        case 0x35: x[0] = svc_signal_addr(x[0], (u32)x[1], (i32)(u32)x[2], (i32)(u32)x[3]); return;
        case 0x45: {
            KEvent* e = kevent_new("user event");
            KWEvent* we = calloc(1, sizeof *we);
            we->o.type = KO_WEVENT;
            we->o.destroy = wev_destroy;
            we->o.name = "user event (writable)";
            we->ev = e;
            kobj_ref(&e->o);
            x[0] = 0;
            x[1] = h_new(&we->o);
            x[2] = h_new(&e->o);
            return;
        }
        case 0x4B: {
            x[0] = R_NOTIMPL;
            return;
        }
        case 0x6F: {  
            u32 ty = (u32)x[1];
            u64 sub = x[3], o = 0;
            if (ty == 0)
                o = sub == 0   ? mem_total()
                    : sub == 1 ? 0x1FB00000ull
                    : sub == 2 ? 0x29D00000ull
                               : 0x6000000ull;  
            else if (ty == 1)
                o = sub == 0 ? used_memory() : 0x1000000ull;  
            else if (ty == 2)
                o = sub == 0 ? 1 : 0x50;  
            x[0] = 0;
            x[1] = o;
            return;
        }
        default:
            WARN("SVC 0x%x not implemented (pc=%llx)\n", imm, (unsigned long long)c->pc);
            x[0] = R_NOTIMPL;
            return;
    }
}
