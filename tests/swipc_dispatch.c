/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/hle/kernel/ipc.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "swipc_dispatch.c:%d: check failed: %s\n", __LINE__, #x);                                  \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)


static int unrefs, stubs, quiets, unknowns, bad_requests, bad_objects;
static const char* registered[4];
static int nregistered;
static SvcObj in_thing;
SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t size) {
    SvcObj* o = calloc(1, sizeof *o);
    o->name = name;
    o->dispatch = dispatch;
    o->refs = 1;
    if (size) o->data = calloc(1, size);
    return o;
}
void svcobj_ref(SvcObj* o) { o->refs++; }
void svcobj_unref(SvcObj* o) {
    if (!o) return;
    unrefs++;
    if (--o->refs) return;
    free(o->data);
    free(o);
}
void out_copy_h(IpcCtx* c, u32 h) { c->out_copy[c->nout_copy++] = h; }
void out_move_h(IpcCtx* c, u32 h) { c->out_move[c->nout_move++] = h; }
void out_obj(IpcCtx* c, SvcObj* o) { c->out_objs[c->nout_objs++] = o; }
int stub_ok(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    stubs++;
    return 0;
}
int quiet_ok(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    quiets++;
    return 0;
}
int ipc_bad_request(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    bad_requests++;
    return 0x3480B;  
}
int ipc_bad_objects(SvcObj* o, IpcCtx* c) {
    (void)o;
    bad_objects++;
    return c->is_domain ? 0x1D60A   : 0x20A  ;
}
int ipc_unknown(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    unknowns++;
    return 0x1BA0A;
}
void svc_register(const char* name, SvcCtor ctor) {
    (void)ctor;
    registered[nregistered++] = name;
}
SvcObj* ipc_in_obj(IpcCtx* c, int i) {
    if (i >= c->nin_objs || c->in_objs[i] != 7) return NULL;
    in_thing.refs++;
    return &in_thing;
}
void ipc_out_overflow(IpcCtx* c) { (void)c; }


#include "tests/swipc/other.swipc.inc"
#include "tests/swipc/test.swipc.inc"
static struct {
    u32 a, f;
    u64 b;
    u8 c;
    u16 d;
    u8 e[3];
    u64 pid, aruid;
    u32 v;
    IpcBuf bufs[8];
    SvcObj* thing;
    u32 ha, hb, hc;
    test_Pair p;
    u8 q, raw[6];
    test_Uid u;
    int empty, fallbacks;
} got;
static SvcObj* ctor(void) { return NULL; }
static int test_IThing_Layout(SvcObj* self, u32 a, u64 b, u8 c, u16 d, const test_Small* e, u32 f, u8* x, u64* y,
                              u16* z) {
    (void)self;
    got.a = a, got.b = b, got.c = c, got.d = d, got.f = f;
    memcpy(got.e, e->v, 3);
    *x = 0x11, *y = 0x2222222222222222ull, *z = 0x3333;
    return 0;
}
static int test_IThing_Pid(SvcObj* self, u32 a, u64 pid, u64 b, u64* pid_out) {
    (void)self;
    got.a = a, got.pid = pid, got.b = b;
    *pid_out = pid;
    return 0;
}
static int test_IThing_Aruid(SvcObj* self, u64 aruid, u32 v, test_Id* id) {
    (void)self;
    got.aruid = aruid, got.v = v;
    *id = 0x99;
    return 0;
}
static int test_IThing_Buffers(SvcObj* self, IpcBuf a0, IpcBuf auto0, IpcBuf x1, IpcBuf b0, IpcBuf auto1, IpcBuf c1,
                               IpcBuf x2, IpcBuf b2) {
    (void)self;
    got.bufs[0] = a0, got.bufs[1] = auto0, got.bufs[2] = x1, got.bufs[3] = b0;
    got.bufs[4] = auto1, got.bufs[5] = c1, got.bufs[6] = x2, got.bufs[7] = b2;
    return 0;
}
static int test_IThing_Objects(SvcObj* self, SvcObj* thing, u32* m, SvcObj** out, u32* h) {
    (void)self;
    got.thing = thing;
    CHECK(thing == &in_thing && thing->refs == 2);  
    *m = 0x51, *h = 0x52;
    *out = svcobj_new("out", NULL, 0);
    return 0;
}
static int test_IThing_Fail(SvcObj* self, SvcObj** o, u32* h, u32* v) {
    (void)self;
    *o = svcobj_new("lost", NULL, 0);
    *h = 0x61;
    *v = 5;
    return 0x1234;
}
static int test_IThing_InHandles(SvcObj* self, u32 a, u32 b, u32 c) {
    (void)self;
    got.ha = a, got.hb = b, got.hc = c;
    return 0;
}
static int test_IThing_Struct(SvcObj* self, const test_Pair* p, u8 q, const test_Uid* u, const u8* raw, test_Pair* r,
                              u8* quad) {
    (void)self;
    got.p = *p, got.q = q, got.u = *u;
    memcpy(got.raw, raw, 6);
    r->a = 0xA1, r->b = 0xB2B2B2B2, r->c = 0xC3C3C3C3C3C3C3C3ull;
    quad[0] = 1, quad[1] = 2, quad[2] = 3, quad[3] = 4;
    return 0;
}
static int test_IThing_Small(SvcObj* self, u8* flag) {
    (void)self;
    *flag = 0x5A;
    return 0;
}
static int test_IThing_Big(SvcObj* self, u8* big) {
    (void)self;
    memset(big, 0xB1, 0xd0);
    return 0;
}
static int test_IThing_Empty(SvcObj* self) {
    (void)self;
    got.empty++;
    return 7;
}
static int test_IThing_ObjectFail(SvcObj* self, SvcObj* thing, u32* v) {
    (void)self;
    got.thing = thing;
    *v = 1;
    return 0x4321;
}
static int test_IThing_RawFail(SvcObj* self, u32 a, u32* v) {
    (void)self;
    got.a = a;
    *v = 0xFFFFFFFF;
    return 0x5678;
}
static int test_IThing_Unsorted(SvcObj* self, const test_Uid* u, u32 a, u8 b, test_Tail* t, u8* c) {
    (void)self;
    got.u = *u, got.a = a, got.c = b;
    t->x = 0x71, t->b = 0x72;
    memset(t->a.v, 0x73, sizeof t->a.v);
    *c = 0x74;
    return 0;
}
static int test_IThing_FixedOut(SvcObj* self, IpcBuf uid) {
    (void)self;
    got.bufs[0] = uid;
    return 0;
}
static int test_fallback(SvcObj* self, IpcCtx* c) {
    (void)self, (void)c;
    got.fallbacks++;
    return 0x2A;
}
static int test_IOther_Swap(SvcObj* self, const test_Pair* p, test_Id* id) {
    (void)self;
    *id = p->c;
    return 0;
}


static IpcCtx ctx;
static u8 raw[0x100];
static void request(u32 cmd) {
    memset(&ctx, 0, sizeof ctx);
    memset(&got, 0, sizeof got);
    ctx.cmd = cmd;
    ctx.raw = raw;
    ctx.raw_size = sizeof raw;
}
static void put(u32 off, const void* p, u32 n) { memcpy(raw + off, p, n); }
static u64 out_u(u32 off, u32 n) {
    u64 v = 0;
    memcpy(&v, ctx.out + off, n);
    return v;
}
static IpcBuf buf(u64 addr, u64 size) {
    IpcBuf b = {addr, size, 0};
    return b;
}
static int same(IpcBuf a, u64 addr, u64 size) { return a.addr == addr && a.size == size; }

int main(void) {
    SvcObj* thing = test_IThing_new(0);
    u32 v32;
    u64 v64;
    u16 v16;
    CHECK(!strcmp(thing->name, "test::IThing"));
    test_IThing_register(ctor);
    CHECK(nregistered == 2 && !strcmp(registered[0], "test:t") && !strcmp(registered[1], "test:u"));


    request(0);
    memset(raw, 0, sizeof raw);
    raw[0] = 0xCC;
    put(1, "\x01\x02\x03", 3);
    v16 = 0xDDDD, put(4, &v16, 2);
    v32 = 0xAAAAAAAA, put(8, &v32, 4);
    v32 = 0xFFFFFFFF, put(12, &v32, 4);
    v64 = 0xBBBBBBBBBBBBBBBBull, put(16, &v64, 8);
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(got.a == 0xAAAAAAAA && got.b == 0xBBBBBBBBBBBBBBBBull && got.c == 0xCC && got.d == 0xDDDD &&
          got.f == 0xFFFFFFFF && !memcmp(got.e, "\x01\x02\x03", 3));
    CHECK(ctx.out_size == 16 && out_u(0, 1) == 0x11 && out_u(2, 2) == 0x3333 && out_u(8, 8) == 0x2222222222222222ull);


    request(1);
    memset(raw, 0, sizeof raw);
    v32 = 5, put(0, &v32, 4);
    v64 = 0xDEAD, put(8, &v64, 8);
    v64 = 6, put(16, &v64, 8);
    ctx.pid = 0x51, ctx.has_pid = 1;
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(got.a == 5 && got.pid == 0x51 && got.b == 6 && ctx.out_size == 8 && out_u(0, 8) == 0x51);
    ctx.has_pid = 0;  
    bad_requests = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && bad_requests == 1 && got.pid == 0x51);


    request(2);
    memset(raw, 0, sizeof raw);
    v32 = 3, put(0, &v32, 4);
    v64 = 0x77, put(8, &v64, 8);
    ctx.pid = 0x51, ctx.has_pid = 1;
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(got.v == 3 && got.aruid == 0x77 && out_u(0, 8) == 0x99);


    request(3);
    ctx.na = 2, ctx.a[0] = buf(0x1000, 1), ctx.a[1] = buf(0, 0);
    ctx.nx = 3, ctx.x[0] = buf(0x2000, 2), ctx.x[1] = buf(0x3000, 3), ctx.x[2] = buf(0x4000, 0x10);
    ctx.nb = 3, ctx.b[0] = buf(0x5000, 4), ctx.b[1] = buf(0x6000, 5), ctx.b[2] = buf(0x6800, 8);
    ctx.nc = 2, ctx.c[0] = buf(0x7000, 6), ctx.c[1] = buf(0x8000, 7);
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(same(got.bufs[0], 0x1000, 1));
    CHECK(same(got.bufs[1], 0x2000, 2));  
    CHECK(same(got.bufs[2], 0x3000, 3));
    CHECK(same(got.bufs[3], 0x5000, 4));
    CHECK(same(got.bufs[4], 0x6000, 5));  
    CHECK(same(got.bufs[5], 0x8000, 7));
    CHECK(same(got.bufs[6], 0x4000, 0x10));
    CHECK(same(got.bufs[7], 0x6800, 8));
    request(3);  
    ctx.na = 2, ctx.nx = 3, ctx.nb = 3, ctx.b[1] = buf(0, 0);
    ctx.nc = 1, ctx.c[0] = buf(0x7000, 6);
    CHECK(thing->dispatch(thing, &ctx) == 0 && same(got.bufs[4], 0x7000, 6));
    request(3);  
    ctx.na = 2, ctx.nx = 3, ctx.nb = 3;
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(same(got.bufs[5], 0, 0) && same(got.bufs[7], 0, 0));

    bad_requests = 0;
    request(3);
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B);
    request(3);
    ctx.na = 2, ctx.nx = 3, ctx.nb = 3, ctx.nw = 1;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && bad_requests == 2);


    request(4);
    in_thing.refs = 1;
    ctx.is_domain = 1, ctx.nin_objs = 1, ctx.in_objs[0] = 7;
    unrefs = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(got.thing == &in_thing && in_thing.refs == 1 && unrefs == 1);
    CHECK(ctx.nout_move == 1 && ctx.out_move[0] == 0x51 && ctx.nout_copy == 1 && ctx.out_copy[0] == 0x52);
    CHECK(ctx.nout_objs == 1 && !strcmp(ctx.out_objs[0]->name, "out") && ctx.out_size == 0);
    svcobj_unref(ctx.out_objs[0]);
    request(4);  
    ctx.is_domain = 1, ctx.nin_objs = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x1D60A && bad_objects == 1 && got.thing == NULL);
    request(4);  
    ctx.is_domain = 1, ctx.na = 1;
    CHECK(thing->dispatch(thing, &ctx) == 0x1D60A && bad_objects == 2);
    request(4);  
    CHECK(thing->dispatch(thing, &ctx) == 0x20A && bad_objects == 3 && got.thing == NULL);
    request(4);
    ctx.na = 1;
    bad_requests = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && bad_requests == 1 && bad_objects == 3);


    request(5);
    CHECK(thing->dispatch(thing, &ctx) == 0x1234);
    CHECK(ctx.out_size == 0 && out_u(0, 4) == 0 && ctx.nout_objs == 1 && !strcmp(ctx.out_objs[0]->name, "lost"));
    CHECK(ctx.nout_copy == 1 && ctx.out_copy[0] == 0x61);
    svcobj_unref(ctx.out_objs[0]);


    request(6);
    CHECK(thing->dispatch(thing, &ctx) == 0 && ctx.out_size == 16 && out_u(0, 8) == 0 && out_u(8, 8) == 0);


    request(7);
    ctx.ncopy = 3, ctx.copy_h[0] = 0x71, ctx.copy_h[1] = 0x72, ctx.copy_h[2] = 0x73;
    CHECK(thing->dispatch(thing, &ctx) == 0 && got.ha == 0x71 && got.hb == 0x72 && got.hc == 0x73);
    request(7);  
    bad_requests = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && got.ha == 0);
    request(7);
    ctx.ncopy = 4;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && bad_requests == 2 && got.ha == 0);


    request(8);
    memset(raw, 0, sizeof raw);
    raw[0] = 0x42;
    put(1, "abcdef", 6);
    raw[8] = 0x0A;
    v32 = 0x0B0B0B0B, put(12, &v32, 4);
    v64 = 0x0C0C0C0C0C0C0C0Cull, put(16, &v64, 8);
    put(24, "0123456789abcdef", 16);
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(got.q == 0x42 && !memcmp(got.raw, "abcdef", 6) && got.p.a == 0x0A && got.p.b == 0x0B0B0B0B &&
          got.p.c == 0x0C0C0C0C0C0C0C0Cull && !memcmp(got.u.v, "0123456789abcdef", 16));
    CHECK(ctx.out_size == 24 && out_u(0, 4) == 0x04030201 && out_u(8, 1) == 0xA1 && out_u(12, 4) == 0xB2B2B2B2 &&
          out_u(16, 8) == 0xC3C3C3C3C3C3C3C3ull);


    request(9);
    CHECK(thing->dispatch(thing, &ctx) == 7 && got.empty == 1);


    request(0);
    ctx.raw_size = 0x17;
    bad_requests = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B && bad_requests == 1 && got.a == 0);
    request(0);
    ctx.raw_size = 0x18;
    CHECK(thing->dispatch(thing, &ctx) == 0);

    request(1);
    memset(raw, 0, sizeof raw);
    v32 = 5, put(0, &v32, 4);
    v64 = 6, put(8, &v64, 8);
    ctx.tipc = 1, ctx.has_pid = 1, ctx.pid = 0x51, ctx.raw_size = 16;
    CHECK(thing->dispatch(thing, &ctx) == 0 && got.a == 5 && got.b == 6 && got.pid == 0x51);
    request(1);
    ctx.tipc = 1, ctx.has_pid = 1, ctx.raw_size = 15;
    CHECK(thing->dispatch(thing, &ctx) == 0x3480B);


    request(10);
    CHECK(thing->dispatch(thing, &ctx) == 0 && ctx.out_size == 4 && out_u(0, 1) == 0x5A);
    request(11);
    CHECK(thing->dispatch(thing, &ctx) == 0 && ctx.out_size == 0xd0 && ctx.out[0xcf] == 0xB1);


    request(12);
    in_thing.refs = 1;
    ctx.is_domain = 1, ctx.nin_objs = 1, ctx.in_objs[0] = 7;
    unrefs = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0x4321);
    CHECK(got.thing == &in_thing && in_thing.refs == 1 && unrefs == 1 && ctx.out_size == 0);

    request(13);
    memset(raw, 0, sizeof raw);
    v32 = 9, put(0, &v32, 4);
    CHECK(thing->dispatch(thing, &ctx) == 0x5678 && got.a == 9 && ctx.out_size == 0 && out_u(0, 4) == 0);


    CHECK(sizeof(test_Tail) == 32 && offsetof(test_Tail, a) == 8 && offsetof(test_Tail, b) == 24);
    request(14);
    memset(raw, 0, sizeof raw);
    put(0, "0123456789abcdef", 16);
    v32 = 0x12345678, put(16, &v32, 4);
    raw[20] = 0x9A;
    CHECK(thing->dispatch(thing, &ctx) == 0);
    CHECK(!memcmp(got.u.v, "0123456789abcdef", 16) && got.a == 0x12345678 && got.c == 0x9A);
    CHECK(ctx.out_size == 36 && out_u(0, 1) == 0x71 && out_u(1, 7) == 0 && out_u(8, 8) == 0x7373737373737373ull &&
          out_u(24, 1) == 0x72 && out_u(25, 7) == 0 && out_u(32, 1) == 0x74);


    request(15);
    ctx.nc = 1, ctx.c[0] = buf(0x9000, 0x40);
    CHECK(thing->dispatch(thing, &ctx) == 0 && same(got.bufs[0], 0x9000, 0x10));
    request(15);
    ctx.nc = 1, ctx.c[0] = buf(0x9000, 8);
    CHECK(thing->dispatch(thing, &ctx) == 0 && same(got.bufs[0], 0x9000, 8));


    {
        SvcObj* o = test_IOther_new(0);
        request(0);
        memset(raw, 0, sizeof raw);
        v64 = 0xC0FFEE, put(8, &v64, 8);
        CHECK(o->dispatch(o, &ctx) == 0 && ctx.out_size == 8 && out_u(0, 8) == 0xC0FFEE);
        svcobj_unref(o);
    }


    request(99);
    stubs = quiets = 0;
    CHECK(thing->dispatch(thing, &ctx) == 0 && stubs == 1);
    {
        SvcObj* q = test_IQuiet_new(0);
        request(0);
        CHECK(q->dispatch(q, &ctx) == 0 && quiets == 1 && stubs == 1);
        svcobj_unref(q);
        q = test_IStrict_new(0);
        CHECK(q->dispatch(q, &ctx) == 0x1BA0A && unknowns == 1);
        svcobj_unref(q);
        q = test_ICustom_new(0);
        CHECK(q->dispatch(q, &ctx) == 0x2A && got.fallbacks == 1);
        svcobj_unref(q);
    }
    svcobj_unref(thing);
    puts("swipc dispatch: ok");
    return 0;
}
