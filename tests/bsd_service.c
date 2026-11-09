/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/hle/services/services_int.h"
#include <stdarg.h>

#include "nx/hle/services/network/bsd.c"

u8* g_mem;
int g_verbose;
int g_opt_network;
static unsigned failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "bsd_service.c:%d: check failed: %s\n", __LINE__, #x);                                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

void pm_init(PMutex* m) { (void)m; }
void pm_lock(PMutex* m) { (void)m; }
void pm_unlock(PMutex* m) { (void)m; }
void logf_(int level, const char* format, ...) {
    (void)level;
    (void)format;
}
SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t size) {
    SvcObj* o = calloc(1, sizeof *o);
    o->name = name;
    o->dispatch = dispatch;
    o->refs = 1;
    if (size) o->data = calloc(1, size);
    return o;
}
void svcobj_unref(SvcObj* o) {
    if (!o || --o->refs) return;
    if (o->destroy) o->destroy(o);
    free(o->data);
    free(o);
}
void ipc_out_overflow(IpcCtx* c) {
    (void)c;
    failures++;
}
int ipc_warn_once(const char* name, u32 cmd) {
    (void)name, (void)cmd;
    return 0;
}

int ipc_bad_request(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return RC(11, 420);
}

int ipc_bad_objects(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return RC(10, 235);
}
int stub_ok(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return 0;
}
void svc_register(const char* name, SvcCtor ctor) { (void)name, (void)ctor; }


enum { IN0 = 0x1000, IN1 = 0x2000, OUT0 = 0x3000, OUT1 = 0x4000, BUFSIZE = 0x1000 };
static SvcObj* bsd;
static IpcCtx ctx;
static u32 raw[16];  
static void request(u32 cmd, u32 a0, u32 a1, u32 a2) {
    memset(&ctx, 0, sizeof ctx);
    memset(raw, 0, sizeof raw);
    raw[0] = a0;
    raw[1] = a1;
    raw[2] = a2;
    ctx.raw = (const u8*)raw;
    ctx.raw_size = sizeof raw;
    ctx.cmd = cmd;
}

static void in(u64 addr, u64 size) {
    ctx.a[ctx.na].addr = addr;
    ctx.a[ctx.na++].size = size;
    ctx.nx++;
}
static void out(u64 addr, u64 size) {
    ctx.b[ctx.nb].addr = addr;
    ctx.b[ctx.nb++].size = size;
    ctx.nc++;
}

static i32 call(u32* err) {
    CHECK(bsd->dispatch(bsd, &ctx) == 0);
    u32 expected = ctx.cmd == 0                                                                         ? 4
                   : (ctx.cmd == 9 || ctx.cmd == 12 || ctx.cmd == 15 || ctx.cmd == 16 || ctx.cmd == 17) ? 12
                                                                                                        : 8;
    CHECK(ctx.out_size == expected);
    i32 r;
    memcpy(&r, ctx.out, 4);
    if (err) memcpy(err, ctx.out + 4, 4);
    return r;
}
static u32 word(int i) {
    u32 v;
    memcpy(&v, ctx.out + i * 4, 4);
    return v;
}

static void sockaddr_lo(u64 at, u16 port_be) {
    u8 s[16] = {16, 2, 0, 0, 127, 0, 0, 1};
    memcpy(s + 2, &port_be, 2);
    memcpy(GP(at), s, 16);
}

int main(void) {
    g_mem = calloc(1, 0x10000);
    network_init();
    snx_net_set_log(NULL);
    bsd = new_bsd_u();
    u32 e;

    request(0, 0, 0, 0);  
    ctx.has_pid = 1;
    ctx.ncopy = 1;
    call(NULL);
    request(2, 2, 1, 0);  
    i32 l = call(&e);
    CHECK(l >= 0 && !e);
    u32 one = 1;
    memcpy(GP(IN0), &one, 4);
    request(21, (u32)l, 0xFFFF, 0x4);  
    in(IN0, 4);
    CHECK(call(&e) == 0);
    request(21, (u32)l, 0xFFFF, 0x10000);  
    in(IN0, 4);
    CHECK(call(&e) == 0);
    sockaddr_lo(IN0, 0);
    request(13, (u32)l, 0, 0);  
    in(IN0, 16);
    CHECK(call(&e) == 0);
    request(18, (u32)l, 4, 0);  
    CHECK(call(&e) == 0);
    request(16, (u32)l, 0, 0);  
    out(OUT0, 16);
    CHECK(call(&e) == 0 && word(2) == 16);
    u8* name = GP(OUT0);
    u16 port;
    memcpy(&port, name + 2, 2);
    CHECK(name[0] == 16 && name[1] == 2 && port && name[4] == 127);

    request(2, 2, 1 | 0x20000000, 0);  
    i32 c = call(&e);
    CHECK(c > l);
    request(20, (u32)c, 3, 0);  
    CHECK((call(&e) & 0x800) == 0x800);
    sockaddr_lo(IN0, port);
    request(14, (u32)c, 0, 0);  
    in(IN0, 16);
    i32 r = call(&e);
    CHECK(r == 0 || (r == -1 && e == SNX_EINPROGRESS));
    request(12, (u32)l, 0, 0);  
    out(OUT0, 16);
    i32 s = call(&e);
    CHECK(s > c && word(2) == 16);


    u8 pfd[8] = {0};
    memcpy(pfd, &c, 4);
    pfd[4] = 0x04;  
    memcpy(GP(IN0), pfd, 8);
    request(6, 1, 2000, 0);
    in(IN0, 8);
    out(OUT0, 8);
    CHECK(call(&e) == 1 && (GP(OUT0)[6] & 0x04));
    memcpy(GP(IN1), "switch", 6);
    request(10, (u32)s, 0, 0);  
    in(IN1, 6);
    CHECK(call(&e) == 6);
    pfd[4] = 0x01;
    memcpy(GP(IN0), pfd, 8);
    request(6, 1, 2000, 0);
    in(IN0, 8);
    out(OUT0, 8);
    CHECK(call(&e) == 1 && (GP(OUT0)[6] & 0x01));
    request(19, (u32)c, BSD_FIONREAD, 1);  
    for (int k = 0; k < 4; k++) in(0, 0);
    out(OUT1, 4);
    for (int k = 0; k < 3; k++) out(0, 0);
    CHECK(call(&e) == 0 && gr32(OUT1) == 6);
    request(8, (u32)c, 0x2, 0);  
    out(OUT0, 3);
    CHECK(call(&e) == 3);
    request(25, (u32)c, 0, 0);  
    out(OUT0, BUFSIZE);
    CHECK(call(&e) == 6 && !memcmp(GP(OUT0), "switch", 6));
    request(8, (u32)c, 0x80, 0);  
    out(OUT0, BUFSIZE);
    CHECK(call(&e) == -1 && e == SNX_EAGAIN);


    u8 sets[3][16];
    memset(sets, 0, sizeof sets);
    sets[0][s / 8] |= (u8)(1 << (s % 8));
    sets[1][c / 8] |= (u8)(1 << (c % 8));
    memcpy(GP(IN0), sets[0], 16);
    memcpy(GP(IN1), sets[1], 16);
    request(5, (u32)(s > c ? s : c) + 1, 0, 0);  
    in(IN0, 16);
    in(IN1, 16);
    in(0, 0);
    out(OUT0, 16);
    out(OUT1, 16);
    out(0, 0);
    CHECK(call(&e) == 1 && !(GP(OUT0)[s / 8] & (1 << (s % 8))) && (GP(OUT1)[c / 8] & (1 << (c % 8))));


    request(17, (u32)s, 0xFFFF, 0x1008);  
    out(OUT0, 4);
    CHECK(call(&e) == 0 && gr32(OUT0) == 1 && word(2) == 4);
    request(17, (u32)s, 0xFFFF, 0x1007);  
    out(OUT0, 4);
    CHECK(call(&e) == 0 && gr32(OUT0) == 0);
    u64 tv[2] = {0, 250000};
    memcpy(GP(IN0), tv, 16);
    request(21, (u32)s, 0xFFFF, 0x1006);  
    in(IN0, 16);
    CHECK(call(&e) == 0);
    request(8, (u32)s, 0, 0);  
    out(OUT0, BUFSIZE);
    CHECK(call(&e) == -1 && e == SNX_EAGAIN);


    memcpy(GP(IN1), "pointer", 7);
    request(10, (u32)c, 0, 0);
    in(0, 0);
    ctx.x[0] = (IpcBuf){IN1, 7, 0};
    CHECK(call(&e) == 7);
    memset(GP(OUT0), 0, 16);
    request(8, (u32)s, 0, 0);  
    out(0, 0);
    ctx.c[0] = (IpcBuf){OUT0, BUFSIZE, 0};
    CHECK(call(&e) == 7 && !memcmp(GP(OUT0), "pointer", 7));
    memset(GP(OUT1), 0, 16);
    request(16, (u32)l, 0, 0);
    out(0, 0);
    ctx.c[0] = (IpcBuf){OUT1, 16, 0};
    CHECK(call(&e) == 0 && word(2) == 16 && GP(OUT1)[1] == 2 && !memcmp(GP(OUT1) + 2, &port, 2));


    request(29, (u32)s, 0, 0);
    CHECK(call(&e) == -1 && e == SNX_EOPNOTSUPP);
    request(100, 0, 0, 0);
    CHECK(bsd->dispatch(bsd, &ctx) == 0 && ctx.out_size == 8 && word(0) == 0xFFFFFFFFu && word(1) == SNX_EOPNOTSUPP);


    request(2, 2, 2, 17);
    i32 u1 = call(&e);
    request(2, 2, 2, 17);
    i32 u2 = call(&e);
    sockaddr_lo(IN0, 0);
    request(13, (u32)u1, 0, 0);
    in(IN0, 16);
    CHECK(call(&e) == 0);
    request(16, (u32)u1, 0, 0);
    out(OUT1, 16);  
    CHECK(call(&e) == 0);
    memcpy(GP(IN1), "udp!", 4);
    request(11, (u32)u2, 0, 0);  
    in(IN1, 4);
    in(OUT1, 16);
    CHECK(call(&e) == 4);
    request(9, (u32)u1, 0, 0);  
    out(OUT0, 64);
    out(IN0, 16);
    CHECK(call(&e) == 4 && word(2) == 16 && GP(IN0)[1] == 2 && GP(IN0)[4] == 127);


    request(11, (u32)u2, 0, 0);
    in(IN1, 4);
    in(OUT1, 16);
    CHECK(call(&e) == 4);
    memset(GP(IN0), 0xA5, 16);
    request(9, (u32)u1, 0, 0);
    out(OUT0, 0);
    out(IN0, 16);
    CHECK(call(&e) == 0 && word(2) == 0 && GP(IN0)[0] == 0xA5);

    {
        u8 ready[8] = {0};
        memcpy(ready, &u1, 4);
        ready[4] = 0x01;  
        memcpy(GP(IN0), ready, 8);
        request(6, 1, 2000, 0);
        in(IN0, 8);
        out(OUT0, 8);
        CHECK(call(&e) == 1 && (GP(OUT0)[6] & 0x01));
    }
    request(9, (u32)u1, 0x80, 0);
    out(OUT0, 64);
    out(IN0, 16);
    CHECK(call(&e) == 4 && word(2) == 16);



    const u32 bad_fd_commands[][3] = {{8, 0, 1},  {9, 0, 2},  {10, 1, 0}, {11, 2, 0}, {12, 0, 1},
                                      {15, 0, 1}, {16, 0, 1}, {17, 0, 1}, {24, 1, 0}, {25, 0, 1}};
    for (unsigned i = 0; i < sizeof bad_fd_commands / sizeof *bad_fd_commands; i++) {
        request(bad_fd_commands[i][0], BSD_MAX_FD, 0, 0);
        for (u32 k = 0; k < bad_fd_commands[i][1]; k++) in(k ? IN0 : IN1, k ? 16 : 4);
        for (u32 k = 0; k < bad_fd_commands[i][2]; k++) out(k ? IN0 : OUT0, k ? 16 : 64);
        CHECK(call(&e) == -1 && e == SNX_EBADF);
        if (ctx.out_size == 12) CHECK(word(2) == 0);
    }


    request(31, 0x4, 0, 2);  
    i32 ev = call(&e);
    CHECK(ev > 0);
    request(25, (u32)ev, 0, 0);
    out(OUT0, 8);
    CHECK(call(&e) == 8 && gr32(OUT0) == 2);
    request(26, (u32)c, 0, 0);
    CHECK(call(&e) == 0);
    request(26, (u32)c, 0, 0);
    CHECK(call(&e) == -1 && e == SNX_EBADF);
    request(2, 2, 3, 6);  
    CHECK(call(&e) == -1 && e == SNX_EINVAL);
    u8 google[16] = {16, 2, 0, 53, 8, 8, 8, 8};
    memcpy(GP(IN0), google, 16);
    request(14, (u32)u2, 0, 0);  
    in(IN0, 16);
    CHECK(call(&e) == -1 && e == SNX_ENETUNREACH);
    CHECK(!network_online());

    svcobj_unref(bsd);
    free(g_mem);
    if (failures) {
        fprintf(stderr, "bsd_service: %u checks failed\n", failures);
        return 1;
    }
    puts("bsd_service: ok");
    return 0;
}
