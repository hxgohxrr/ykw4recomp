/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/hle/kernel/kernel_int.h"
#include "nx/runtime/options.h"


SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t ds) {
    SvcObj* o = calloc(1, sizeof *o);
    o->name = name;
    o->dispatch = dispatch;
    o->refs = 1;
    if (ds) o->data = calloc(1, ds);
    return o;
}
void svcobj_ref(SvcObj* o) { __atomic_add_fetch(&o->refs, 1, __ATOMIC_SEQ_CST); }
void svcobj_unref(SvcObj* o) {
    if (!o) return;
    if (__atomic_sub_fetch(&o->refs, 1, __ATOMIC_SEQ_CST) == 0) {
        if (o->destroy) o->destroy(o);
        free(o->data);
        free(o);
    }
}
static void domain_unref(Domain* d) {
    if (!d) return;
    pm_lock(&d->lk);
    int r = --d->refs;
    pm_unlock(&d->lk);
    if (r) return;
    for (int i = 0; i < d->n; i++)
        if (d->obj[i]) svcobj_unref(d->obj[i]);
    free(d->obj);
    free(d);
}
static void sess_destroy(KObj* k) {
    KSession* s = (KSession*)k;
    svcobj_unref(s->obj);
    domain_unref(s->domain);
    free(k->waiters);
    free(s);
}
static u32 domain_add(Domain* d, SvcObj* o) {
    pm_lock(&d->lk);
    int id = -1;
    for (int i = 1; i < d->n; i++)
        if (!d->obj[i]) {
            id = i;
            break;
        }  
    if (id < 0) {
        if (d->n == d->cap) {
            d->cap = d->cap ? d->cap * 2 : 16;
            d->obj = realloc(d->obj, sizeof(SvcObj*) * d->cap);
        }
        id = d->n++;
    }
    d->obj[id] = o;
    pm_unlock(&d->lk);
    return (u32)id + 1;
}
static KSession* session_alloc(SvcObj* obj) {
    KSession* s = calloc(1, sizeof *s);
    s->o.type = KO_SESSION;
    s->o.destroy = sess_destroy;
    s->o.name = "session";
    s->obj = obj;
    return s;
}
u32 session_new(SvcObj* obj) { return h_new(&session_alloc(obj)->o); }


u32 ipc_user_buffer(u64 a, u64 size) {
    if (a & 0xFFF) return R_BADADDR;
    if (!size || (size & 0xFFF)) return R_BADSIZE;
    if (a + size < a || a + size > AS_SIZE) return R_BADMEMSTATE;
    u32 state = 0;
    for (u64 end = a + size; a < end;) {
        MemBlock b;
        mem_query(a, &b);
        if (!state) state = b.state;  
        if (!(b.state & (1u << 21)) || b.state != state || b.perm != PERM_RW || b.attr || b.addr + b.size <= a)
            return R_BADMEMSTATE;
        a = b.addr + b.size;
    }
    return 0;
}


int ipc_warn_once(const char* name, u32 cmd) {
    static struct {
        char n[128];
        u32 cmd, cnt;
    } t[512];
    static int nt;
    static PMutex lk = PMUTEX_INIT;
    pm_lock(&lk);
    int r = 1, i = 0;
    for (; i < nt; i++)
        if (t[i].cmd == cmd && !strncmp(t[i].n, name, sizeof t[i].n - 1)) {
            if (t[i].cnt < 4) t[i].cnt++;  
            r = t[i].cnt <= 3;
            break;
        }
    if (i == nt && nt < 512) {
        snprintf(t[nt].n, sizeof t[nt].n, "%s", name);
        t[nt].cmd = cmd;
        t[nt].cnt = 1;
        nt++;
    } else if (i == nt) {  
        r = 0;
        WARN_LIMIT(1, "IPC: more than 512 service commands unhandled, the rest aren't logged\n");
    }
    pm_unlock(&lk);
    return r;
}

int stub_ok(SvcObj* o, IpcCtx* c) {
    if (!g_opt_auto_stub_ipc) return ipc_unknown(o, c);
    if (ipc_warn_once(o->name, c->cmd)) LOG("  [undeclared] %s cmd %u -> ok\n", o->name, c->cmd);
    return 0;
}
int quiet_ok(SvcObj* o, IpcCtx* c) {
    (void)o;
    (void)c;
    return 0;
}
void ipc_out_overflow(IpcCtx* c) {
    WARN_LIMIT(1, "IPC: the reply to cmd %u is larger than the message buffer, cut short\n", c->cmd);
}


typedef struct {
    char name[16];
    SvcCtor ctor;
} SvcReg;
static SvcReg regs[256];
static int nregs;
void svc_register(const char* n, SvcCtor c) {
    if (nregs == 256) fatal("too many services registered (%s)", n);
    strncpy(regs[nregs].name, n, 15);
    regs[nregs].ctor = c;
    nregs++;
}
SvcObj* svc_open(const char* name) {
    for (int i = 0; i < nregs; i++)
        if (!strcmp(regs[i].name, name)) return regs[i].ctor();
    return NULL;
}
int svc_registered(const char* name) {
    for (int i = 0; i < nregs; i++)
        if (!strcmp(regs[i].name, name)) return 1;
    return 0;
}

void out_copy_h(IpcCtx* c, u32 h) {
    if (c->nout_copy < 8) c->out_copy[c->nout_copy++] = h;
}
void out_move_h(IpcCtx* c, u32 h) {
    if (c->nout_move < 8) c->out_move[c->nout_move++] = h;
}
void out_obj(IpcCtx* c, SvcObj* o) {
    if (c->nout_objs < 8) c->out_objs[c->nout_objs++] = o;
}
int ipc_bad_request(SvcObj* o, IpcCtx* c) {
    if (ipc_warn_once(o->name, c->cmd))
        WARN("  [bad request] %s cmd %u: pid %d, A %d, B %d, X %d, W %d, copy %d, 0x%x bytes of data is not what it "
             "declares -> InvalidCmifRequest\n",
             o->name, c->cmd, c->has_pid, c->na, c->nb, c->nx, c->nw, c->ncopy, c->raw_size);
    return RC(11, 420);  
}
int ipc_bad_objects(SvcObj* o, IpcCtx* c) {
    if (!c->is_domain) {  
        if (ipc_warn_once(o->name, c->cmd))
            WARN("  [bad request] %s cmd %u takes objects, on a session that isn't a domain -> NotSupported\n", o->name,
                 c->cmd);
        return RC(10, 1);  
    }
    if (ipc_warn_once(o->name, c->cmd))
        WARN("  [bad request] %s cmd %u: %d objects is not what it declares -> InvalidNumInObjects\n", o->name, c->cmd,
             c->nin_objs);
    return RC(10, 235);  
}
int ipc_unknown(SvcObj* o, IpcCtx* c) {
    if (ipc_warn_once(o->name, c->cmd)) WARN("  [unknown] %s cmd %u -> UnknownCommandId\n", o->name, c->cmd);
    return RC(10, 221);  
}

static void drop_outputs(IpcCtx* c) {
    for (int i = 0; i < c->nout_copy; i++) h_close(c->out_copy[i]);
    for (int i = 0; i < c->nout_move; i++) h_close(c->out_move[i]);
    for (int i = 0; i < c->nout_objs; i++) svcobj_unref(c->out_objs[i]);
    c->nout_copy = c->nout_move = c->nout_objs = 0;
    c->out_size = 0;
}

char g_ipc_ring[32][160];
u32 g_ipc_ring_n;

static u32 g_ipc_ring_seq[32];
static u32 ring_claim(void) {
    u32 seq = __atomic_fetch_add(&g_ipc_ring_n, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipc_ring_seq[seq & 31], seq, __ATOMIC_RELEASE);
    return seq;
}
static int ring_mine(u32 seq) { return __atomic_load_n(&g_ipc_ring_seq[seq & 31], __ATOMIC_ACQUIRE) == seq; }


static void check_buffer(IpcCtx* c, const IpcBuf* b) {
    if (b->size && (b->addr > AS_SIZE || b->size > AS_SIZE - b->addr)) c->kernel_rc = R_BADMEMSTATE;
}

static int cmif_in_header(const u8* d) {
    u32 version;
    memcpy(&version, d + 4, 4);
    return !memcmp(d, "SFCI", 4) && version <= 1;
}

static void parse(IpcCtx* c) {
    u8* m = c->msg;
    u32 w0, w1;
    memcpy(&w0, m, 4);
    memcpy(&w1, m + 4, 4);
    c->type = w0 & 0xFFFF;
    u32 nx = (w0 >> 16) & 15, na = (w0 >> 20) & 15, nb = (w0 >> 24) & 15, nw = (w0 >> 28) & 15;
    u32 ndata = w1 & 0x3FF, cflags = (w1 >> 10) & 15, recv_off = (w1 >> 20) & 0x7FF;  
    int special = (w1 >> 31) & 1;
    u32 nc = cflags == 0 ? 0 : cflags == 1 ? 0 : cflags == 2 ? 1 : cflags - 2;
    u8* p = m + 8;
    {

        u32 sh = 0, raw_end, all;
        if (special) memcpy(&sh, p, 4);
        raw_end = 8 + (special ? 4 + ((sh & 1) ? 8 : 0) + 4 * (((sh >> 1) & 15) + ((sh >> 5) & 15)) : 0) + 8 * nx +
                  12 * (na + nb + nw) + 4 * ndata;
        all = (recv_off ? recv_off * 4 : raw_end) + 8 * nc;
        if (all > c->msg_size)
            c->kernel_rc = R_BADCOMBO;
        else if (raw_end > 0x100)
            c->kernel_rc = R_MSGTOOLARGE;
        else if ((sh >> 5) & 15)
            c->kernel_rc = R_BADCOMBO;
        if (c->kernel_rc) return;
    }
    if (special) {
        u32 sh;
        memcpy(&sh, p, 4);
        p += 4;
        c->has_pid = sh & 1;
        u32 ncopy = (sh >> 1) & 15;  
        if (c->has_pid) {

            c->pid = K_PROCESS_ID;
            p += 8;
        }
        for (u32 i = 0; i < ncopy; i++, p += 4)
            if (i < 8) memcpy(&c->copy_h[i], p, 4);
        c->ncopy = ncopy < 8 ? (int)ncopy : 8;
    }
    for (u32 i = 0; i < nx; i++, p += 8) {  
        u32 a, b;
        IpcBuf x;
        memcpy(&a, p, 4);
        memcpy(&b, p + 4, 4);
        x.addr = b | ((u64)((a >> 12) & 15) << 32) | ((u64)((a >> 6) & 7) << 36);
        x.size = a >> 16;
        x.flags = 0;
        check_buffer(c, &x);
        if (i < MAX_BUFS) c->x[i] = x;
    }
    c->nx = nx < MAX_BUFS ? (int)nx : MAX_BUFS;
    IpcBuf* lists[3] = {c->a, c->b, c->w};
    u32 cnt[3] = {na, nb, nw};
    for (int k = 0; k < 3; k++)
        for (u32 i = 0; i < cnt[k]; i++, p += 12) {
            u32 s, a, f;
            IpcBuf d;
            memcpy(&s, p, 4);
            memcpy(&a, p + 4, 4);
            memcpy(&f, p + 8, 4);
            d.addr = a | ((u64)((f >> 28) & 15) << 32) | ((u64)((f >> 2) & 7) << 36);
            d.size = s | ((u64)((f >> 24) & 15) << 32);
            d.flags = f & 3;
            check_buffer(c, &d);
            if (i < MAX_BUFS) lists[k][i] = d;
        }
    c->na = na < MAX_BUFS ? (int)na : MAX_BUFS;
    c->nb = nb < MAX_BUFS ? (int)nb : MAX_BUFS;
    c->nw = nw < MAX_BUFS ? (int)nw : MAX_BUFS;

    if (c->kernel_rc) return;
    if (recv_off && recv_off * 4 < (u32)(p - m) + 4 * ndata) {
        c->kernel_rc = R_BADCOMBO;
        return;
    }

    u8* raw = m + (((p - m) + 15) & ~15);

    u8* cp = recv_off ? m + recv_off * 4 : p + ndata * 4;
    c->nc = nc < MAX_BUFS ? (int)nc : MAX_BUFS;
    for (u32 i = 0; i < nc; i++, cp += 8) {
        u32 a, b;
        IpcBuf r;
        memcpy(&a, cp, 4);
        memcpy(&b, cp + 4, 4);
        r.addr = a | ((u64)(b & 0xFFFF) << 32);
        r.size = b >> 16;
        r.flags = 0;
        check_buffer(c, &r);
        if (i < MAX_BUFS) c->c[i] = r;
    }

    u8* end = p + ndata * 4;

    if (c->type >= 16) {
        c->tipc = 1;
        c->cmd = c->type - 16;
        c->raw = p;
        c->raw_size = (u32)(end - p);
        return;
    }

    end = ndata * 4 >= 16 ? raw + ndata * 4 - 16 : raw;
    u8* d = raw;
    u32 dlen_max = 0x100;  
    c->is_domain = 0;
    if (__atomic_load_n(&c->sess->domain, __ATOMIC_ACQUIRE) && (c->type == 4 || c->type == 6)) {
        if (end < d + 16) {    
            c->is_domain = 2;  
            c->header_rc = RC(10, 202);
            return;
        }
        u8 dcmd = d[0];
        u8 nobj = d[1];
        u16 dlen;
        memcpy(&dlen, d + 2, 2);
        memcpy(&c->domain_obj, d + 4, 4);
        c->is_domain = 1 + dcmd;  

        u32 avail = end > d + 16 ? (u32)(end - (d + 16)) : 0;
        if ((u32)dlen + nobj * 4u > avail)
            c->domain_rc = RC(10, 202);  
        else if (nobj > 8)
            c->domain_rc = RC(10, 235);  
        else {
            c->nin_objs = nobj;
            for (int i = 0; i < c->nin_objs; i++) memcpy(&c->in_objs[i], d + 16 + dlen + i * 4, 4);
        }
        dlen_max = dlen > 16 ? dlen - 16u : 0;  
        d += 16;

        if (!c->domain_rc && dlen < 16) c->domain_rc = RC(10, 202);           
        if (!c->domain_rc && !cmif_in_header(d)) c->domain_rc = RC(10, 211);  
    } else if (end < d + 16 || !cmif_in_header(d)) {                          
        c->header_rc = end < d + 16 ? RC(10, 202)   : RC(10, 211)  ;
        return;
    }
    memcpy(&c->cmd, d + 8, 4);
    memcpy(&c->token, d + 12, 4);
    c->raw = d + 16;
    c->raw_size = end > c->raw ? (u32)(end - c->raw) : 0;
    if (c->raw_size > dlen_max) c->raw_size = dlen_max;
}


static void respond(IpcCtx* c) {
    u8* m = c->msg;
    memset(m, 0, 0x100);
    int dom = c->is_domain != 0 && !c->tipc;
    u32 nhandles = c->nout_copy + c->nout_move;
    if (!dom) nhandles += c->nout_objs;  
    u32 w0 = 0;
    u8* p = m + 8;
    u32 special = (c->nout_copy || c->nout_move || (!dom && c->nout_objs)) ? 1 : 0;
    u32 hdr_words = 2 + (special ? 1 + nhandles : 0);
    u32 pad_bytes = ((hdr_words * 4 + 15) & ~15) - hdr_words * 4;

    u32 payload = (c->out_size + 3) & ~3u;

    u32 room = (u32)(c->msg_size < 0x100 ? c->msg_size : 0x100) - hdr_words * 4 -
               (c->tipc ? 4 : pad_bytes + 16 + (dom ? 16 + c->nout_objs * 4 : 0));  
    if (payload > room) {
        ipc_out_overflow(c);
        payload = room & ~3u;
    }
    u32 data_bytes = 16 + payload + (dom ? 16 + c->nout_objs * 4 : 0);

    u32 ndata = (16 + data_bytes + 3) / 4;
    memcpy(m, &w0, 4);
    u32 w1 = ndata | (special ? 0x80000000u : 0);
    memcpy(m + 4, &w1, 4);
    if (special) {
        u32 nmove = c->nout_move + (dom ? 0 : c->nout_objs);
        u32 sh = (c->nout_copy << 1) | (nmove << 5);
        memcpy(p, &sh, 4);
        p += 4;
        for (int i = 0; i < c->nout_copy; i++) {
            memcpy(p, &c->out_copy[i], 4);
            p += 4;
        }

        if (!dom)
            for (int i = 0; i < c->nout_objs; i++) {
                u32 h = c->out_objs[i] ? session_new(c->out_objs[i]) : 0;
                memcpy(p, &h, 4);
                p += 4;
            }
        for (int i = 0; i < c->nout_move; i++) {
            memcpy(p, &c->out_move[i], 4);
            p += 4;
        }
    }
    if (c->tipc) {  
        w1 = (1 + payload / 4) | (special ? 0x80000000u : 0);
        memcpy(m + 4, &w1, 4);
        memcpy(p, &c->rc, 4);
        memcpy(p + 4, c->out, payload);
        return;
    }
    u8* d = m + (((p - m) + 15) & ~15);
    if (dom) {
        u32 n = c->nout_objs;
        memcpy(d, &n, 4);
        d += 16;
    }
    u32 magic = 0x4F434653;
    memcpy(d, &magic, 4);
    u32 ver = 0;
    memcpy(d + 4, &ver, 4);
    memcpy(d + 8, &c->rc, 4);
    memcpy(d + 12, &c->token, 4);
    memcpy(d + 16, c->out, payload);
    if (dom) {
        u8* o = d + 16 + payload;
        for (int i = 0; i < c->nout_objs; i++) {
            u32 id =
                c->out_objs[i] ? domain_add(__atomic_load_n(&c->sess->domain, __ATOMIC_ACQUIRE), c->out_objs[i]) : 0;
            memcpy(o + i * 4, &id, 4);
        }
    }
}

static SvcObj* domain_get(KSession* s, u32 id) {
    Domain* d = __atomic_load_n(&s->domain, __ATOMIC_ACQUIRE);
    if (!d) return NULL;
    pm_lock(&d->lk);
    SvcObj* o = (id && id <= (u32)d->n) ? d->obj[id - 1] : NULL;
    if (o) svcobj_ref(o);
    pm_unlock(&d->lk);
    return o;
}
SvcObj* ipc_in_obj(IpcCtx* c, int i) {
    if (i >= c->nin_objs) return NULL;
    return domain_get(c->sess, c->in_objs[i]);
}


static void control(IpcCtx* c, KSession* s) {
    switch (c->cmd) {
        case 0: {  
            if (!__atomic_load_n(&s->domain, __ATOMIC_ACQUIRE)) {

                Domain *d = calloc(1, sizeof *d), *none = NULL;
                pm_init(&d->lk);
                d->refs = 1;
                svcobj_ref(s->obj);
                domain_add(d, s->obj);
                if (!__atomic_compare_exchange_n(&s->domain, &none, d, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                    domain_unref(d);
            }
            out32(c, 1);
            break;
        }
        case 1: {  
            u32 id;
            ipc_in(c, 0, &id, 4);
            if (!__atomic_load_n(&s->domain, __ATOMIC_ACQUIRE)) {
                c->rc = RC(11, 491);  
                break;
            }
            SvcObj* o = domain_get(s, id);  
            if (!o) {
                c->rc = RC(11, 492);  
                break;
            }
            out_move_h(c, session_new(o));
            break;
        }
        case 2:
        case 4: {  
            svcobj_ref(s->obj);
            KSession* ns = session_alloc(s->obj);
            Domain* d = __atomic_load_n(&s->domain, __ATOMIC_ACQUIRE);
            if (d) {
                pm_lock(&d->lk);
                d->refs++;
                pm_unlock(&d->lk);
                ns->domain = d;
            }
            out_move_h(c, h_new(&ns->o));
            break;
        }
        case 3: out32(c, 0x8000); break;  
        default:
            if (ipc_warn_once("IPC control", c->cmd)) WARN("IPC: control command %u not handled\n", c->cmd);
            c->rc = RC(10, 221);  
            break;
    }
}

void ipc_handle(KThread* t, u32 handle, u64 msg_addr, u64 msg_size) {
    CPU* cpu = &t->cpu;

    KSession* s = (KSession*)h_get_ref(handle, KO_SESSION);
    if (!s) {
        WARN_LIMIT(8, "SendSyncRequest to invalid handle %x (pc=%llx)\n", handle, (unsigned long long)cpu->pc);
        cpu->x[0] = R_BADHANDLE;
        u32 seq = ring_claim();
        snprintf(g_ipc_ring[seq & 31], sizeof g_ipc_ring[0], "handle %x -> %x (not a session)", handle, R_BADHANDLE);
        return;
    }
    if (__atomic_load_n(&s->closed, __ATOMIC_ACQUIRE)) {  
        cpu->x[0] = R_SESSIONCLOSED;
        u32 seq = ring_claim();
        snprintf(g_ipc_ring[seq & 31], sizeof g_ipc_ring[0], "%s -> %x (session closed)", s->obj->name,
                 R_SESSIONCLOSED);
        kobj_unref(&s->o);
        return;
    }
    IpcCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    IpcCtx* c = &ctx;
    const char* name = s->obj->name;  
    int served = 0;                   
    SvcObj* held = NULL;              
    c->msg = GP(msg_addr);
    c->msg_size = msg_size;
    c->sess = s;
    parse(c);
    cpu->x[0] = 0;
    if (c->kernel_rc) {  
        WARN_LIMIT(8, "IPC: request to %s refused by the kernel (%x): %s\n", s->obj->name, c->kernel_rc,
                   c->kernel_rc == R_BADCOMBO
                       ? "its header describes more than its buffer, a receive list inside its data, or a move handle"
                   : c->kernel_rc == R_MSGTOOLARGE ? "the message is larger than the server's buffer"
                                                   : "a buffer is outside the address space");
        cpu->x[0] = c->kernel_rc;
        goto out;
    }

    if (c->type != 4 && c->type != 5 && c->type != 6 && c->type != 7 && c->type < 16) {
        if (c->type != 2 && c->type != 15)
            WARN_LIMIT(4, "IPC: request to %s of message type %u, which sf doesn't serve: session closed\n", name,
                       c->type);
        __atomic_store_n(&s->closed, 1, __ATOMIC_RELEASE);
        cpu->x[0] = R_SESSIONCLOSED;
        goto out;
    }
    if (c->header_rc) {  
        WARN_LIMIT(8, "IPC: request to %s with a broken header -> %x\n", s->obj->name, c->header_rc);
        c->rc = c->header_rc;
        respond(c);
        goto out;
    }
    if (c->type == 5 || c->type == 7) {  
        control(c, s);
        respond(c);
        goto out;
    }
    SvcObj* obj = s->obj;

    if (c->is_domain) {
        if (c->is_domain == 3) {  
            Domain* d = __atomic_load_n(&s->domain, __ATOMIC_ACQUIRE);
            SvcObj* old = NULL;
            if (d) {
                pm_lock(&d->lk);
                if (c->domain_obj && c->domain_obj <= (u32)d->n) {
                    old = d->obj[c->domain_obj - 1];
                    d->obj[c->domain_obj - 1] = NULL;
                }
                pm_unlock(&d->lk);
            }
            if (old) svcobj_unref(old);
            c->is_domain = 1;
            respond(c);
            goto out;
        }
        if (c->is_domain != 2) {  
            WARN_LIMIT(8, "IPC: domain request type %u to %s\n", c->is_domain - 1, name);
            c->rc = RC(10, 211);  
            respond(c);
            goto out;
        }
        obj = held = domain_get(s, c->domain_obj);
        if (!obj) {
            WARN_LIMIT(8, "IPC: domain object %u of %s isn't there\n", c->domain_obj, name);
            c->rc = RC(10, 261);  
            respond(c);
            goto out;
        }
        name = obj->name;
        if (c->domain_rc) {  
            WARN_LIMIT(8, "IPC: domain request to object %u (%s): bad header (%x)\n", c->domain_obj, name,
                       c->domain_rc);
            c->rc = c->domain_rc;
            respond(c);
            goto out;
        }
    }
    LOGV("IPC %s cmd %u\n", obj->name, c->cmd);
    {

        u32 in0, in1;
        ipc_in(c, 0, &in0, 4);
        ipc_in(c, 4, &in1, 4);
        u32 seq = ring_claim();
        snprintf(g_ipc_ring[seq & 31], sizeof g_ipc_ring[0], "%s cmd %u (in %x %x) -> running", name, c->cmd, in0, in1);
        c->rc = (u32)obj->dispatch(obj, c);
        if (ring_mine(seq))
            snprintf(g_ipc_ring[seq & 31], sizeof g_ipc_ring[0], "%s cmd %u (in %x %x) -> %x", name, c->cmd, in0, in1,
                     c->rc);
        served = 1;
    }
    if (c->rc) drop_outputs(c);
    respond(c);
out:
    if (!served) {  
        u32 rc = (u32)cpu->x[0] ? (u32)cpu->x[0] : c->rc, seq = ring_claim();
        snprintf(g_ipc_ring[seq & 31], sizeof g_ipc_ring[0], "%s type %u cmd %u -> %x (not served)", name, c->type,
                 c->cmd, rc);
    }
    if (held) svcobj_unref(held);  
    kobj_unref(&s->o);
}
