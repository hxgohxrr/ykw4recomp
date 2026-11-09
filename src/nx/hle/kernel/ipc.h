/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_IPC_H
#define SNX_IPC_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/hle/kernel/kernel.h"
#include <string.h>

#define MAX_BUFS 8
typedef struct {
    u64 addr, size;
    u32 flags;
} IpcBuf;
struct SvcObj;
typedef struct IpcCtx {
    u8* msg;
    u64 msg_size;
    u32 type;
    u32 cmd;
    u32 token;
    const u8* raw;
    u32 raw_size;  
    u64 pid;
    int has_pid;
    u32 copy_h[8];  
    int ncopy;
    IpcBuf a[MAX_BUFS], b[MAX_BUFS], w[MAX_BUFS], x[MAX_BUFS], c[MAX_BUFS];
    int na, nb, nw, nx, nc;
    u32 kernel_rc;  
    u32 domain_rc;  
    u32 header_rc;  
    int is_domain;
    u32 domain_obj;
    u32 in_objs[8];
    int nin_objs;
    int tipc;

    u32 rc;
    u8 out[0x100];
    u32 out_size;
    u32 out_copy[8], out_move[8];
    int nout_copy, nout_move;
    struct SvcObj* out_objs[8];
    int nout_objs;
    struct KSession* sess;
} IpcCtx;

typedef struct SvcObj {
    const char* name;
    int (*dispatch)(struct SvcObj* self, IpcCtx* ctx);
    void (*destroy)(struct SvcObj* self);
    int refs;
    void* data;
    u64 u[8];
} SvcObj;

typedef struct KSession {
    KObj o;
    SvcObj* obj;

    struct Domain* domain;
    int closed;  
} KSession;
typedef struct Domain {
    SvcObj** obj;
    int n, cap, refs;
    PMutex lk;
} Domain;

SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t datasize);
void svcobj_ref(SvcObj* o);
void svcobj_unref(SvcObj* o);
u32 session_new(SvcObj* obj);

u32 ipc_user_buffer(u64 addr, u64 size);

void ipc_handle(KThread* t, u32 handle, u64 msg_addr, u64 msg_size);

void ipc_out_overflow(IpcCtx* c);
static inline int out_fits(IpcCtx* c, u32 n) {
    if (n <= sizeof c->out - c->out_size) return 1;
    ipc_out_overflow(c);
    return 0;
}
static inline void out32(IpcCtx* c, u32 v) {
    if (!out_fits(c, 4)) return;
    memcpy(c->out + c->out_size, &v, 4);
    c->out_size += 4;
}
void out_copy_h(IpcCtx* c, u32 h);
void out_move_h(IpcCtx* c, u32 h);
void out_obj(IpcCtx* c, SvcObj* o);

static inline void ipc_in(IpcCtx* c, u32 off, void* dst, u32 n) {
    if (off + n <= c->raw_size)
        memcpy(dst, c->raw + off, n);
    else
        memset(dst, 0, n);
}
static inline void ipc_out(IpcCtx* c, u32 off, const void* src, u32 n) { memcpy(c->out + off, src, n); }
static inline IpcBuf ipc_buf(const IpcBuf* list, int n, int i) {
    IpcBuf none = {0, 0, 0};
    return i < n ? list[i] : none;
}

static inline IpcBuf ipc_buf_auto(const IpcBuf* map, int nmap, int i, const IpcBuf* ptr, int nptr, int j) {
    IpcBuf b = ipc_buf(map, nmap, i);
    return b.addr ? b : ipc_buf(ptr, nptr, j);
}

static inline int ipc_counts(const IpcCtx* c, int pid, int na, int nb, int nx, int ncopy, u32 raw, u32 tipc_raw) {
    return c->has_pid == pid && c->na == na && c->nb == nb && c->nx == nx && !c->nw && c->ncopy == ncopy &&
           c->raw_size >= (c->tipc ? tipc_raw : raw);
}

static inline int ipc_objects(const IpcCtx* c, int nobj) { return !c->is_domain || c->nin_objs == nobj; }

int ipc_bad_request(SvcObj* o, IpcCtx* c);  

int ipc_bad_objects(SvcObj* o, IpcCtx* c);
SvcObj* ipc_in_obj(IpcCtx* c, int i);  
#define RC(mod, desc) ((mod) | ((desc) << 9))
#define RC_OK         0
int stub_ok(SvcObj* o, IpcCtx* c);      
int quiet_ok(SvcObj* o, IpcCtx* c);     
int ipc_unknown(SvcObj* o, IpcCtx* c);  

int ipc_warn_once(const char* name, u32 cmd);
extern char g_ipc_ring[32][160];  
extern u32 g_ipc_ring_n;


typedef SvcObj* (*SvcCtor)(void);
void svc_register(const char* name, SvcCtor ctor);
SvcObj* svc_open(const char* name);
int svc_registered(const char* name);

#endif
