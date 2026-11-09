/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/hle/services/network/network.h"
#include <stdarg.h>
#include <time.h>

#include "hle/services/network/bsd.swipc.h"
#include "nx/hle/services/network/ssl.c"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

u8* g_mem;
int g_verbose;
int g_opt_network;
static unsigned test_objects;
static void test_fail(const char* expression, int line) {
    fprintf(stderr, "ssl_service.c:%d: check failed: %s\n", line, expression);
    exit(1);
}
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) test_fail(#expr, __LINE__);                                                                       \
    } while (0)

#ifdef _WIN32
void pm_init(PMutex* m) { InitializeSRWLock(&m->l); }
void pm_lock(PMutex* m) { AcquireSRWLockExclusive(&m->l); }
void pm_unlock(PMutex* m) { ReleaseSRWLockExclusive(&m->l); }
u64 plat_ns(void) { return (u64)GetTickCount64() * 1000000; }
#else
void pm_init(PMutex* m) { CHECK(!pthread_mutex_init(&m->l, NULL)); }
void pm_lock(PMutex* m) { CHECK(!pthread_mutex_lock(&m->l)); }
void pm_unlock(PMutex* m) { CHECK(!pthread_mutex_unlock(&m->l)); }
u64 plat_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (u64)t.tv_sec * 1000000000 + (u64)t.tv_nsec;
}
#endif
void logf_(int level, const char* format, ...) {
    (void)level;
    (void)format;
}
void fatal(const char* format, ...) {
    va_list a;
    va_start(a, format);
    vfprintf(stderr, format, a);
    va_end(a);
    exit(1);
}
SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t size) {
    SvcObj* obj = calloc(1, sizeof *obj);
    CHECK(obj != NULL);
    obj->name = name;
    obj->dispatch = dispatch;
    obj->refs = 1;
    if (size) obj->data = calloc(1, size);
    test_objects++;
    return obj;
}
void svcobj_ref(SvcObj* obj) { obj->refs++; }
void svcobj_unref(SvcObj* obj) {
    if (!obj || --obj->refs) return;
    if (obj->destroy) obj->destroy(obj);
    free(obj->data);
    free(obj);
    test_objects--;
}
void out_obj(IpcCtx* c, SvcObj* obj) {
    CHECK(c->nout_objs < 8);
    c->out_objs[c->nout_objs++] = obj;
}
void ipc_out_overflow(IpcCtx* c) {
    (void)c;
    test_fail("IPC output overflow", __LINE__);
}
int ipc_warn_once(const char* name, u32 cmd) {
    (void)name, (void)cmd;
    return 0;
}
int ipc_bad_request(SvcObj* o, IpcCtx* c) {
    fprintf(stderr, "bad request: %s command %u\n", o->name, c->cmd);
    test_fail("a request that doesn't match its declaration", __LINE__);
    return RC(11, 420);
}
int ipc_bad_objects(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return RC(10, 235);
}
int stub_ok(SvcObj* o, IpcCtx* c) {
    fprintf(stderr, "unexpected stub: %s command %u\n", o->name, c->cmd);
    test_fail("a command without a handler", __LINE__);
    return -1;
}


enum { BUF = 0x10000, BUF_A = 0x10000, BUF_B = 0x20000, BUFSIZE = 0x8000, PID = 0x51 };
static IpcCtx ctx;
static u8 raw[64];  
static void request(u32 cmd) {
    memset(&ctx, 0, sizeof ctx);
    memset(raw, 0, sizeof raw);
    ctx.raw = raw;
    ctx.raw_size = sizeof raw;
    ctx.cmd = cmd;
}
static void arg32(u32 off, u32 v) { memcpy(raw + off, &v, 4); }
static void in_buf(u64 addr, u64 size) {
    ctx.a[ctx.na].addr = addr;
    ctx.a[ctx.na++].size = size;
}
static void out_buf(u64 addr, u64 size) {
    ctx.b[ctx.nb].addr = addr;
    ctx.b[ctx.nb++].size = size;
}
static u32 reply32(u32 off) {
    u32 v;
    memcpy(&v, ctx.out + off, 4);
    return v;
}
static u64 reply64(u32 off) {
    u64 v;
    memcpy(&v, ctx.out + off, 8);
    return v;
}
static int call(SvcObj* o) { return o->dispatch(o, &ctx); }
#define SSL_RESULT(desc) RC(123, desc)


static SvcObj* bsd;
static i32 game_socket(u16 port) {
    u8 sin[16] = {16, 2, 0, 0, 127, 0, 0, 1};
    i32 fd;
    request(2);  
    arg32(0, 2);
    arg32(4, 1);
    CHECK(call(bsd) == 0);
    fd = (i32)reply32(0);
    CHECK(fd >= 0);
    memcpy(sin + 2, &port, 2);
    memcpy(GP(BUF_A), sin, 16);
    request(14);  
    arg32(0, (u32)fd);
    ctx.a[0].addr = BUF_A;  
    ctx.a[0].size = 16;
    ctx.na = 1;
    ctx.nx = 1;
    CHECK(call(bsd) == 0);
    CHECK(reply32(0) == 0);
    return fd;
}
static void game_close(i32 fd) {
    request(26);
    arg32(0, (u32)fd);
    CHECK(call(bsd) == 0);
    CHECK(reply32(0) == 0);
}



static EVP_PKEY* server_key;
static X509* server_cert;
static void make_cert(void) {
    X509V3_CTX v3;
    X509_EXTENSION* san;
    X509_NAME* name;
    server_key = EVP_EC_gen("P-256");
    CHECK(server_key != NULL);
    server_cert = X509_new();
    CHECK(server_cert && X509_set_version(server_cert, X509_VERSION_3));
    ASN1_INTEGER_set(X509_get_serialNumber(server_cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(server_cert), -3600);
    X509_gmtime_adj(X509_getm_notAfter(server_cert), 86400);
    CHECK(X509_set_pubkey(server_cert, server_key));
    name = X509_NAME_new();
    CHECK(name && X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"localhost", -1, -1, 0));
    CHECK(X509_set_subject_name(server_cert, name) && X509_set_issuer_name(server_cert, name));
    X509_NAME_free(name);
    X509V3_set_ctx(&v3, server_cert, server_cert, NULL, NULL, 0);
    san = X509V3_EXT_conf_nid(NULL, &v3, NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");
    CHECK(san && X509_add_ext(server_cert, san, -1));
    X509_EXTENSION_free(san);
    CHECK(X509_sign(server_cert, server_key, EVP_sha256()) > 0);
}

static int sbio_write(BIO* b, const char* data, size_t size, size_t* written) {
    int64_t r = snx_socket_send(BIO_get_data(b), data, size, 0, NULL);
    if (r < 0) return 0;
    *written = (size_t)r;
    return 1;
}
static int sbio_read(BIO* b, char* data, size_t size, size_t* got) {
    int64_t r = snx_socket_recv(BIO_get_data(b), data, size, 0, NULL, NULL);
    if (r <= 0) return 0;
    *got = (size_t)r;
    return 1;
}
static long sbio_ctrl(BIO* b, int cmd, long num, void* ptr) {
    (void)b, (void)num, (void)ptr;
    return cmd == BIO_CTRL_FLUSH;
}
static int sbio_create(BIO* b) {
    BIO_set_init(b, 1);
    return 1;
}

enum { SERVER_CONNECTIONS = 5 };
static SnxSocket* listener;
static int server_handshakes;
#ifdef _WIN32
static DWORD WINAPI server(void* arg) {
#else
static void* server(void* arg) {
#endif
    SSL_CTX* sctx = SSL_CTX_new(TLS_server_method());
    BIO_METHOD* m = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "test server");
    int i;
    (void)arg;
    CHECK(sctx && m);
    CHECK(SSL_CTX_use_certificate(sctx, server_cert) == 1 && SSL_CTX_use_PrivateKey(sctx, server_key) == 1);
    BIO_meth_set_write_ex(m, sbio_write);
    BIO_meth_set_read_ex(m, sbio_read);
    BIO_meth_set_ctrl(m, sbio_ctrl);
    BIO_meth_set_create(m, sbio_create);
    for (i = 0; i < SERVER_CONNECTIONS; i++) {
        SnxSocket* s;
        SSL* ssl;
        BIO* b;
        char line[256];
        size_t n;
        CHECK(snx_socket_accept(listener, &s, NULL, 0) == 0);
        ssl = SSL_new(sctx);
        b = BIO_new(m);
        CHECK(ssl && b);
        BIO_set_data(b, s);
        SSL_set_bio(ssl, b, b);
        if (SSL_accept(ssl) == 1) {
            __atomic_add_fetch(&server_handshakes, 1, __ATOMIC_SEQ_CST);
            while (SSL_read_ex(ssl, line, sizeof line, &n) == 1) {
                if (n == 5 && !memcmp(line, "close", 5)) {
                    SSL_shutdown(ssl);
                    break;
                }
                CHECK(SSL_write_ex(ssl, line, n, &n) == 1);
            }
        }
        SSL_free(ssl);
        snx_socket_unref(s);
    }
    BIO_meth_free(m);
    SSL_CTX_free(sctx);
    return 0;
}


static SvcObj* new_context(SvcObj* ssl) {
    request(0);  
    arg32(0, 1);
    ctx.has_pid = 1;
    ctx.pid = PID;
    CHECK(call(ssl) == 0);
    CHECK(ctx.nout_objs == 1);
    return ctx.out_objs[0];
}
static SvcObj* new_connection(SvcObj* context) {
    request(2);
    CHECK(call(context) == 0);
    CHECK(ctx.nout_objs == 1);
    return ctx.out_objs[0];
}
static void set_socket(SvcObj* c, i32 fd, i32 expect) {
    request(0);
    arg32(0, (u32)fd);
    CHECK(call(c) == 0);
    CHECK((i32)reply32(0) == expect);
}
static void set_host(SvcObj* c, const char* host) {
    strcpy((char*)GP(BUF_A), host);
    request(1);
    in_buf(BUF_A, strlen(host) + 1);
    CHECK(call(c) == 0);
}
static void set_u32(SvcObj* c, u32 cmd, u32 v) {
    request(cmd);
    arg32(0, v);
    CHECK(call(c) == 0);
}
static int handshake(SvcObj* c) {
    request(8);
    return call(c);
}
static int ssl_write(SvcObj* c, const char* text, u32* n) {
    int r;
    memcpy(GP(BUF_A), text, strlen(text));
    request(11);
    in_buf(BUF_A, strlen(text));
    r = call(c);
    *n = r ? 0 : reply32(0);
    return r;
}

static int ssl_read(SvcObj* c, u32 cmd, u32 size, u32* n) {
    int r;
    request(cmd);
    out_buf(BUF_B, size);
    r = call(c);
    *n = r ? 0 : reply32(0);
    return r;
}

int main(void) {
    SvcObj *ssl, *context, *other, *c;
    SnxSockAddr lo;
    u8 der[4096];
    u16 port;
    i32 fd;
    u32 n;
    int r;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
    g_mem = calloc(1, 0x40000);
    CHECK(g_mem != NULL);
    network_init();
    snx_net_set_log(NULL);
    make_cert();


    CHECK(snx_socket_open(&listener, SNX_AF_INET, SNX_SOCK_STREAM, 0) == 0);
    memset(&lo, 0, sizeof lo);
    lo.family = SNX_AF_INET;
    lo.addr[0] = 127;
    lo.addr[3] = 1;
    CHECK(snx_socket_bind(listener, &lo) == 0);
    CHECK(snx_socket_listen(listener, 8) == 0);
    CHECK(snx_socket_name(listener, &lo) == 0);
    port = lo.port;
#ifdef _WIN32
    thread = CreateThread(NULL, 0, server, NULL, 0, NULL);
    CHECK(thread != NULL);
#else
    CHECK(!pthread_create(&thread, NULL, server, NULL));
#endif


    bsd = new_bsd_u();
    request(0);
    ctx.has_pid = 1;
    ctx.pid = PID;
    ctx.ncopy = 1;
    CHECK(call(bsd) == 0);

    ssl = new_ssl();

    memset(GP(BUF_A), 0xFF, 4);  
    request(3);
    in_buf(BUF_A, 4);
    CHECK(call(ssl) == 0 && reply32(0) == 0x18);
    request(2);
    out_buf(BUF_B, 0x18);
    in_buf(BUF_A, 4);
    CHECK(call(ssl) == 0 && reply32(0) == 0);
    CHECK(gr32(BUF_B) == 0xFFFFFFFF && gr32(BUF_B + 4) == 0xFFFFFFFF && gr64(BUF_B + 8) == 0 && gr64(BUF_B + 16) == 0);
    request(2);
    out_buf(BUF_B, 0x10);
    in_buf(BUF_A, 4);
    CHECK(call(ssl) == SSL_RESULT(202));

    context = new_context(ssl);
    CHECK(g_roots || g_roots_dir[0]);  
    request(1);                        
    CHECK(call(ssl) == 0 && reply32(0) == 1);
    request(1);  
    arg32(0, 1);
    CHECK(call(context) == 0 && reply32(0) == 1);
    request(0);  
    arg32(0, 1);
    arg32(4, 0);
    CHECK(call(context) == 0);
    request(1);
    arg32(0, 1);
    CHECK(call(context) == 0 && reply32(0) == 0);
    request(1);
    arg32(0, 7);
    CHECK(call(context) == SSL_RESULT(126));


    c = new_connection(context);
    request(3);  
    CHECK(call(context) == 0 && reply32(0) == 1);
    request(6);  
    CHECK(call(c) == 0 && reply32(0) == 3);
    request(7);  
    CHECK(call(c) == 0 && reply32(0) == 1);
    request(8);
    CHECK(call(c) == SSL_RESULT(103));  
    fd = game_socket(port);
    request(0);
    arg32(0, 99);
    CHECK(call(c) == SSL_RESULT(106));
    set_socket(c, fd, fd);  
    request(0);
    arg32(0, (u32)fd);
    CHECK(call(c) == SSL_RESULT(203));
    request(22);  
    raw[0] = 1;
    arg32(4, 0);
    CHECK(call(c) == SSL_RESULT(126));
    request(4);  
    CHECK(call(c) == 0 && (i32)reply32(0) == fd);
    game_close(fd);  
    set_host(c, "localhost");
    request(5);  
    out_buf(BUF_B, 64);
    CHECK(call(c) == 0 && reply32(0) == 9 && !strcmp((char*)GP(BUF_B), "localhost"));
    CHECK(handshake(c) == SSL_RESULT(308));
    request(15);
    CHECK(call(c) == SSL_RESULT(308));
    request(15);
    CHECK(call(c) == 0);
    svcobj_unref(c);
    request(3);
    CHECK(call(context) == 0 && reply32(0) == 0);


    {
        u8* p = der;
        int len = i2d_X509(server_cert, &p);
        CHECK(len > 0 && len <= (int)sizeof der);
        memcpy(GP(BUF_A), der, (size_t)len);
        request(4);  
        arg32(0, 2);
        in_buf(BUF_A, (u64)len);
        CHECK(call(context) == 0 && reply64(0) != 0);
        request(4);
        arg32(0, 3);
        in_buf(BUF_A, (u64)len);
        CHECK(call(context) == SSL_RESULT(126));
    }


    c = new_connection(context);
    request(22);  
    raw[0] = 1;
    arg32(4, 0);
    CHECK(call(c) == 0);
    request(22);  
    raw[0] = 1;
    arg32(4, 1);
    CHECK(call(c) == 0);
    request(23);
    arg32(0, 1);
    CHECK(call(c) == 0 && ctx.out[0] == 1);
    request(23);
    arg32(0, 2);
    CHECK(call(c) == 0 && ctx.out[0] == 0);
    request(23);
    arg32(0, 9);
    CHECK(call(c) == SSL_RESULT(126));
    fd = game_socket(port);
    set_socket(c, fd, -1);  
    set_host(c, "localhost");
    request(9);  
    out_buf(BUF_B, BUFSIZE);
    CHECK(call(c) == 0);
    {
        u32 size = reply32(0), count = reply32(4);
        u8 got[4096];
        u8* p = got;
        int len = i2d_X509(server_cert, &p);
        CHECK(count == 1 && size == 16 + 8 + (u32)len);
        CHECK(gr64(BUF_B) == 0x4E4D684374726543ull && gr32(BUF_B + 8) == 1);
        CHECK(gr32(BUF_B + 16) == (u32)len && gr32(BUF_B + 20) == 24);
        CHECK(!memcmp(GP(BUF_B + 24), got, (size_t)len));
        request(16);  
        CHECK(call(c) == 0 && reply32(0) == size);
    }
    request(25);  
    arg32(0, 1);
    out_buf(BUF_B, 0x48);
    CHECK(call(c) == 0 && !memcmp(GP(BUF_B), "TLS_", 4) && !strncmp((char*)GP(BUF_B + 0x40), "TLSv1.3", 8));
    CHECK(ssl_write(c, "hello", &n) == 0 && n == 5);
    CHECK(ssl_read(c, 10, 64, &n) == 0 && n == 5 && !memcmp(GP(BUF_B), "hello", 5));  
    set_u32(c, 3, 2);                                                                 
    request(7);
    CHECK(call(c) == 0 && reply32(0) == 2);
    CHECK(ssl_read(c, 10, 64, &n) == SSL_RESULT(204));  
    CHECK(ssl_read(c, 13, 64, &n) == SSL_RESULT(204));
    request(12);  
    CHECK(call(c) == 0 && reply32(0) == 0);
    request(14);  
    arg32(0, 2);
    arg32(4, 0);
    CHECK(call(c) == 0 && reply32(0) == 2);
    CHECK(ssl_write(c, "again", &n) == 0 && n == 5);
    request(14);  
    arg32(0, 1);
    arg32(4, 10000);
    CHECK(call(c) == 0 && reply32(0) == 1);

    for (r = 0; r < 1000; r++) {
        int pr = ssl_read(c, 13, 3, &n);
        if (!pr) break;
        CHECK(pr == SSL_RESULT(204));
        request(14);
        arg32(0, 1);
        arg32(4, 100);
        CHECK(call(c) == 0);
    }
    CHECK(n == 3 && !memcmp(GP(BUF_B), "aga", 3));  
    request(12);
    CHECK(call(c) == 0 && reply32(0) == 5);
    request(14);  
    arg32(0, 1);
    arg32(4, 0);
    CHECK(call(c) == 0 && reply32(0) == 1);
    CHECK(ssl_read(c, 10, 64, &n) == 0 && n == 5 && !memcmp(GP(BUF_B), "again", 5));
    CHECK(ssl_read(c, 10, 64, &n) == SSL_RESULT(204));

    set_u32(c, 3, 1);
    CHECK(ssl_write(c, "close", &n) == 0 && n == 5);
    CHECK(ssl_read(c, 10, 64, &n) == 0 && n == 0);
    svcobj_unref(c);
    game_close(fd);  


    c = new_connection(context);
    fd = game_socket(port);
    set_socket(c, fd, fd);
    game_close(fd);
    set_host(c, "wrong.example");
    CHECK(handshake(c) == SSL_RESULT(301));
    svcobj_unref(c);


    c = new_connection(context);
    fd = game_socket(port);
    set_socket(c, fd, fd);
    game_close(fd);
    set_host(c, "wrong.example");
    set_u32(c, 2, 1);
    CHECK(handshake(c) == 0);
    CHECK(ssl_write(c, "close", &n) == 0);

    CHECK(ssl_read(c, 10, 64, &n) == 0 && n == 0);
    svcobj_unref(c);
    c = new_connection(context);
    fd = game_socket(port);
    set_socket(c, fd, fd);
    game_close(fd);
    set_host(c, "127.0.0.1");
    CHECK(handshake(c) == 0);
    CHECK(ssl_write(c, "close", &n) == 0);
    CHECK(ssl_read(c, 10, 64, &n) == 0 && n == 0);


    other = new_context(ssl);
    request(1);
    CHECK(call(ssl) == 0 && reply32(0) == 2);
    svcobj_unref(other);
    svcobj_unref(c);

#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    CHECK(server_handshakes == 3);
    svcobj_unref(context);
    svcobj_unref(ssl);
    svcobj_unref(bsd);
    snx_socket_unref(listener);
    CHECK(test_objects == 0);
    X509_free(server_cert);
    EVP_PKEY_free(server_key);
    printf("ssl_service: ok\n");
    return 0;
}
