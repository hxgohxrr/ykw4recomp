/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/hle/filesystem/fs.h"
#include "nx/hle/kernel/ipc.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#define test_chdir _chdir
#else
#include <dirent.h>
#include <unistd.h>
#define test_chdir chdir
#endif


#include "nx/hle/filesystem/fs.c"

u8* g_mem;
char g_datadir[1024], g_exename[256], g_exedir[1024];
int g_verbose;
static unsigned test_checks, test_scans, test_objects;
static int test_scans_allowed = 1;

static void test_fail(const char* expression, int line) {
    fprintf(stderr, "filesystem_service.c:%d: check failed: %s\n", line, expression);
    exit(1);
}
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        test_checks++;                                                                                                 \
        if (!(expr)) test_fail(#expr, __LINE__);                                                                       \
    } while (0)

static void test_scan(void) {
    CHECK(test_scans_allowed);
    test_scans++;
}

#if defined(_WIN32)
HANDLE WINAPI fs_test_FindFirstFileA(LPCSTR path, LPWIN32_FIND_DATAA data);
HANDLE WINAPI fs_test_FindFirstFileW(LPCWSTR path, LPWIN32_FIND_DATAW data);
HANDLE WINAPI fs_test_FindFirstFileA(LPCSTR path, LPWIN32_FIND_DATAA data) {
    test_scan();
    return FindFirstFileA(path, data);
}
HANDLE WINAPI fs_test_FindFirstFileW(LPCWSTR path, LPWIN32_FIND_DATAW data) {
    test_scan();
    return FindFirstFileW(path, data);
}
void pm_init(PMutex* m) { InitializeSRWLock(&m->l); }
void pm_lock(PMutex* m) { AcquireSRWLockExclusive(&m->l); }
void pm_unlock(PMutex* m) { ReleaseSRWLockExclusive(&m->l); }
int plat_mkdir(const char* path) { return CreateDirectoryA(path, NULL) ? 0 : -1; }
#else
DIR* fs_test_opendir(const char* path);
DIR* fs_test_opendir(const char* path) {
    test_scan();
    return opendir(path);
}
void pm_init(PMutex* m) { pthread_mutex_init(&m->l, NULL); }
void pm_lock(PMutex* m) { pthread_mutex_lock(&m->l); }
void pm_unlock(PMutex* m) { pthread_mutex_unlock(&m->l); }
int plat_mkdir(const char* path) { return mkdir(path, 0777); }
#endif

int plat_dir_open(PDir* d, const char* path) {
    (void)d;
    (void)path;
    test_fail("HLE attempted platform directory enumeration", __LINE__);
    return -1;
}
int plat_dir_next(PDir* d) {
    (void)d;
    test_fail("HLE attempted platform directory enumeration", __LINE__);
    return 0;
}
void plat_dir_close(PDir* d) { (void)d; }
int plat_is_dir(const char* path) {
    (void)path;
    test_fail("HLE attempted host existence query", __LINE__);
    return 0;
}
u64 plat_file_size(const char* path) {
    (void)path;
    test_fail("HLE attempted host size query", __LINE__);
    return 0;
}
i64 plat_pread(FILE* f, void* buf, u64 size, u64 off) {
#if defined(_WIN32)
    if (_fseeki64(f, (i64)off, SEEK_SET)) return -1;
#else
    if (fseeko(f, (off_t)off, SEEK_SET)) return -1;
#endif
    size_t got = fread(buf, 1, (size_t)size, f);
    return ferror(f) ? -1 : (i64)got;
}
int plat_file_map(PFileMap* map, const char* path) {
    (void)map;
    (void)path;
    test_fail("HLE attempted mapping without its indexed size", __LINE__);
    return -1;
}
int plat_file_map_sized(PFileMap* map, const char* path, u64 size) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    u8* data = malloc(size ? (size_t)size : 1);
    CHECK(data != NULL);
    CHECK(fread(data, 1, (size_t)size, f) == (size_t)size);
    CHECK(!fclose(f));
    map->data = data;
    map->size = size;
    return 0;
}
void plat_file_unmap(PFileMap* map) {
    if (!map->borrowed) free((void*)map->data);
    map->data = NULL;
    map->size = 0;
    map->borrowed = 0;
}
static int test_shared;  
int plat_file_share(const char* path, u64 size, int* fd, const u8** data) {
    *fd = -1;
    *data = NULL;

    size_t length = strlen(path);
    if (length >= 8 && !strcmp(path + length - 8, "zulu.bin")) return -1;
    u8* bytes = NULL;
    if (size) {
        FILE* f = fopen(path, "rb");
        CHECK(f != NULL);
        bytes = malloc((size_t)size);
        CHECK(bytes != NULL);
        CHECK(fread(bytes, 1, (size_t)size, f) == (size_t)size);
        CHECK(!fclose(f));
    }
    *fd = ++test_shared;
    *data = bytes;
    return 0;
}
void plat_file_unshare(int fd, const u8* data, u64 size) {
    (void)size;
    CHECK(fd > 0 && test_shared > 0);
    test_shared--;
    free((void*)data);
}
void logf_(int level, const char* format, ...) {
    (void)level;
    (void)format;
}
void fatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}
void frontend_frame(void) {}

SvcObj* svcobj_new(const char* name, int (*dispatch)(SvcObj*, IpcCtx*), size_t size) {
    SvcObj* obj = calloc(1, sizeof *obj);
    CHECK(obj != NULL);
    obj->name = name;
    obj->dispatch = dispatch;
    obj->refs = 1;
    if (size) {
        obj->data = calloc(1, size);
        CHECK(obj->data != NULL);
    }
    test_objects++;
    return obj;
}
void svcobj_unref(SvcObj* obj) {
    if (!obj || --obj->refs) return;
    if (obj->destroy) obj->destroy(obj);
    free(obj->data);
    free(obj);
    test_objects--;
}

u64 g_title_id = 0x0100000000010000ull;
static int test_events;
KEvent* owned_event(const char* name) {
    KEvent* event = calloc(1, sizeof *event);
    CHECK(event != NULL);
    (void)name;
    event->o.refs = 1;
    test_events++;
    return event;
}
void kobj_unref(KObj* o) {
    if (--o->refs) return;
    free(o);
    test_events--;
}
static KEvent* test_handles[32];  
static unsigned test_nhandles;
u32 ev_handle(KEvent* event) {
    CHECK(test_nhandles < sizeof test_handles / sizeof test_handles[0]);
    event->o.refs++;
    test_handles[test_nhandles] = event;
    return 0x100 + test_nhandles++;
}
void out_copy_h(IpcCtx* c, u32 handle) {
    CHECK(c->nout_copy < 8);
    c->out_copy[c->nout_copy++] = handle;
}
void out_obj(IpcCtx* c, SvcObj* obj) {  
    CHECK(c->nout_objs < 8);
    c->out_objs[c->nout_objs++] = obj;
}
void svc_register(const char* name, SvcCtor ctor) {
    (void)name;
    (void)ctor;
}

int ipc_bad_request(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return RC(11, 420);
}

int ipc_bad_objects(SvcObj* o, IpcCtx* c) {
    (void)o, (void)c;
    return RC(10, 235);
}
int stub_ok(SvcObj* obj, IpcCtx* c) {
    fprintf(stderr, "Unexpected stub: %s command %u\n", obj->name, c->cmd);
    test_fail("requested a command the service does not declare", __LINE__);
    return -1;
}
void ipc_out_overflow(IpcCtx* c) {
    (void)c;
    test_fail("IPC output overflow", __LINE__);
}

typedef struct {
    IpcCtx c;
    u8 raw[0x80];
} Request;

static void request(Request* r, u32 cmd) {
    memset(r, 0, sizeof *r);
    r->c.cmd = cmd;
    r->c.raw = r->raw;
    r->c.raw_size = sizeof r->raw;
}
static void argument32(Request* r, unsigned off, u32 value) { memcpy(r->raw + off, &value, sizeof value); }
static void argument64(Request* r, unsigned off, u64 value) { memcpy(r->raw + off, &value, sizeof value); }
static void argument_path(Request* r, int idx, const char* path) {
    size_t size = strlen(path) + 1;
    CHECK(idx < 2 && size <= 0x1000);
    u64 address = 0x1000 + (u64)idx * 0x1000;
    memcpy(GP(address), path, size);
    r->c.x[idx] = (IpcBuf){address, size, 0};
    r->c.nx = idx + 1;
}
static void output_buffer(Request* r, u64 size) {
    CHECK(size <= 0x8000);
    r->c.b[0] = (IpcBuf){0x5000, size, 0};
    r->c.nb = 1;
    memset(GP(0x5000), 0xcc, (size_t)size);
}
static u64 output64(Request* r) {
    u64 value;
    CHECK(r->c.out_size >= 8);
    memcpy(&value, r->c.out, sizeof value);
    return value;
}

static void dispatch(SvcObj* obj, Request* r, u32 expected) {
    u32 result = (u32)obj->dispatch(obj, &r->c);
    if (result) {
        for (int i = 0; i < r->c.nout_objs; i++) svcobj_unref(r->c.out_objs[i]);
        r->c.nout_copy = r->c.nout_move = r->c.nout_objs = 0;
        r->c.out_size = 0;
    }
    if (result != expected)
        fprintf(stderr, "%s command %u returned %u, expected %u\n", obj->name, r->c.cmd, result, expected);
    CHECK(result == expected);
}
static void fs_command(SvcObj* fs, u32 command, const char* path, u64 value, u32 expected) {
    Request r;
    request(&r, command);
    argument_path(&r, 0, path);
    argument64(&r, 8, value);
    dispatch(fs, &r, expected);
}
static SvcObj* open_object(SvcObj* fs, u32 command, const char* path, u32 mode) {
    Request r;
    request(&r, command);
    argument32(&r, 0, mode);
    argument_path(&r, 0, path);
    dispatch(fs, &r, 0);
    CHECK(r.c.nout_objs == 1);
    return r.c.out_objs[0];
}
static void rename_object(SvcObj* fs, u32 command, const char* source, const char* dest, u32 expected) {
    Request r;
    request(&r, command);
    argument_path(&r, 0, source);
    argument_path(&r, 1, dest);
    dispatch(fs, &r, expected);
}
static u64 get_size(SvcObj* file) {
    Request r;
    request(&r, 4);
    dispatch(file, &r, 0);
    return output64(&r);
}
static void set_size(SvcObj* file, u64 size, u32 expected) {
    Request r;
    request(&r, 3);
    argument64(&r, 0, size);
    dispatch(file, &r, expected);
}
static void write_data(SvcObj* file, u64 offset, const char* data, size_t size, u32 expected) {
    Request r;
    request(&r, 1);
    argument32(&r, 0, 1);
    argument64(&r, 8, offset);
    argument64(&r, 16, size);
    CHECK(size < 0x1000);
    memcpy(GP(0x3000), data, size);
    r.c.a[0] = (IpcBuf){0x3000, size, 0};
    r.c.na = 1;
    dispatch(file, &r, expected);
}
static u64 read_data(SvcObj* file, u64 offset, size_t size, u32 expected) {
    Request r;
    request(&r, 0);
    argument64(&r, 8, offset);
    argument64(&r, 16, size);
    output_buffer(&r, size);
    dispatch(file, &r, expected);
    return expected ? 0 : output64(&r);
}
static u64 dir_count(SvcObj* dir) {
    Request r;
    request(&r, 1);
    dispatch(dir, &r, 0);
    return output64(&r);
}
static u64 dir_read(SvcObj* dir, unsigned capacity) {
    Request r;
    request(&r, 0);
    output_buffer(&r, (u64)capacity * 0x310);
    dispatch(dir, &r, 0);
    return output64(&r);
}

static void test_file_services(SvcObj* fs) {
    fs_command(fs, 0, "/work.bin", 4, 0);
    fs_command(fs, 0, "/work.bin", 4, FS_EXISTS);
    fs_command(fs, 0, "/missing/file.bin", 4, FS_NOTFOUND);
    SvcObj* writer = open_object(fs, 8, "/work.bin", 7);
    SvcObj* reader = open_object(fs, 8, "/work.bin", 1);
    CHECK(get_size(writer) == 4 && get_size(reader) == 4);
    write_data(writer, 4, "hello", 5, 0);
    CHECK(get_size(reader) == 9);
    CHECK(read_data(reader, 0, 16, 0) == 9);
    const u8 expected[9] = {0, 0, 0, 0, 'h', 'e', 'l', 'l', 'o'};
    CHECK(!memcmp(GP(0x5000), expected, sizeof expected));
    CHECK(GP(0x5000)[9] == 0xcc);
    write_data(reader, 0, "x", 1, FS_WRITE_NOT_PERMITTED);
    set_size(reader, 1, FS_WRITE_NOT_PERMITTED);
    SvcObj* fixed = open_object(fs, 8, "/work.bin", 3);
    write_data(fixed, 9, "x", 1, FS_NO_APPEND);
    CHECK(get_size(fixed) == 9);
    set_size(writer, 2, 0);
    CHECK(get_size(reader) == 2 && get_size(fixed) == 2);
    CHECK(read_data(reader, 0, 16, 0) == 2);
    read_data(reader, 3, 1, FS_OUT_OF_RANGE);
    set_size(writer, 8192, 0);
    CHECK(get_size(reader) == 8192 && get_size(fixed) == 8192);
    CHECK(read_data(reader, 8191, 1, 0) == 1 && GP(0x5000)[0] == 0);
    SvcObj* dir = open_object(fs, 9, "/", 2);
    u64 entries = dir_read(dir, 8);
    int found = 0;
    for (u64 i = 0; i < entries; i++) {
        u8* entry = GP(0x5000 + i * 0x310);
        if (!strcmp((char*)entry, "work.bin")) {
            u64 size;
            memcpy(&size, entry + 0x308, sizeof size);
            CHECK(size == 8192 && entry[0x304] == 1);
            found = 1;
        }
    }
    CHECK(found);
    svcobj_unref(dir);
    svcobj_unref(fixed);
    svcobj_unref(reader);
    svcobj_unref(writer);
    rename_object(fs, 5, "/work.bin", "/renamed.bin", 0);
    fs_command(fs, 7, "/work.bin", 0, FS_NOTFOUND);
    reader = open_object(fs, 8, "/renamed.bin", 1);
    CHECK(get_size(reader) == 8192);
    svcobj_unref(reader);
    fs_command(fs, 1, "/renamed.bin", 0, 0);
    fs_command(fs, 7, "/renamed.bin", 0, FS_NOTFOUND);
    puts("PASS HLE create/open/write/read/truncate/rename/delete and shared cached size");
}


static void test_file_contracts(SvcObj* fs) {
    const u64 negative = UINT64_MAX;
    fs_command(fs, 0, "/contract.bin", 4, 0);
    SvcObj* rw = open_object(fs, 8, "/contract.bin", 3);
    SvcObj* ro = open_object(fs, 8, "/contract.bin", 1);
    SvcObj* wo = open_object(fs, 8, "/contract.bin", 2);
    static const struct {
        u64 off, size, capacity;
        u32 result;
    } cases[] = {
        {UINT64_MAX, UINT64_MAX, 4, RC(2, 6061)},  
        {UINT64_MAX, 0, 0, RC(2, 6061)},
        {0, UINT64_MAX, 4, RC(2, 6062)},
        {0, 5, 4, RC(2, 6062)},    
        {100, 5, 4, RC(2, 6062)},  
        {0, 1, 0, RC(2, 6062)},
        {INT64_MAX, 1, 4, RC(2, 3005)},  
        {INT64_MAX - 1, 2, 4, RC(2, 3005)},
        {INT64_MAX, 0, 0, 0},  
        {0, 0, 0, 0},
    };
    for (u32 cmd = 0; cmd < 2; ++cmd) {
        for (unsigned permitted = 0; permitted < 2; ++permitted) {
            SvcObj* target = permitted ? rw : cmd ? ro : wo;
            for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
                Request r;
                request(&r, cmd);
                argument32(&r, 0, cmd);  
                argument64(&r, 8, cases[i].off);
                argument64(&r, 16, cases[i].size);
                output_buffer(&r, 16);
                if (cmd) {  
                    r.c.a[0] = (IpcBuf){0x3000, cases[i].capacity, 0};
                    r.c.na = 1;
                    r.c.nb = 0;
                } else
                    r.c.b[0].size = cases[i].capacity;
                dispatch(target, &r, cases[i].result);
                CHECK(r.c.nout_objs == 0 && r.c.nout_copy == 0 && r.c.nout_move == 0);
                if (!cases[i].result && !cmd)
                    CHECK(r.c.out_size == 8 && output64(&r) == 0);
                else
                    CHECK(r.c.out_size == 0);
                for (unsigned j = 0; j < 16; ++j) CHECK(GP(0x5000)[j] == 0xcc);
                CHECK(get_size(rw) == 4);
            }
        }
    }
    read_data(wo, 0, 1, RC(2, 6202));
    write_data(ro, 0, "x", 1, RC(2, 6203));
    write_data(rw, 4, "x", 1, RC(2, 6201));
    set_size(ro, negative, RC(2, 6062));  
    set_size(rw, negative, RC(2, 6062));
    CHECK(get_size(rw) == 4);
    CHECK(read_data(ro, 0, 4, 0) == 4);
    for (unsigned j = 0; j < 4; ++j) CHECK(GP(0x5000)[j] == 0);
    fs_command(fs, 0, "/new.bin", negative, RC(2, 6062));
    fs_command(fs, 7, "/new.bin", 0, RC(2, 1));
    fs_command(fs, 0, "/../bad", negative, RC(2, 6062));  
    static const u32 modes[] = {0, 4, 8, 0x80000000u};
    static const char* paths[] = {"/contract.bin", "/missing", "/"};
    for (u32 cmd = 8; cmd <= 9; ++cmd)
        for (size_t i = 0; i < sizeof modes / sizeof *modes; ++i)
            for (size_t j = 0; j < sizeof paths / sizeof *paths; ++j) {
                Request r;
                request(&r, cmd);
                argument32(&r, 0, modes[i]);
                argument_path(&r, 0, paths[j]);
                dispatch(fs, &r, RC(2, 6072));
                CHECK(r.c.nout_objs == 0 && r.c.out_size == 0);
            }
    SvcObj* dir = open_object(fs, 9, "/", 0x80000003u);
    svcobj_unref(dir);
    svcobj_unref(wo);
    svcobj_unref(ro);
    svcobj_unref(rw);
    fs_command(fs, 1, "/contract.bin", 0, 0);
    puts("PASS Atmosphere IFile/IFileSystem signed arguments, error priority, empty transfers and open modes");
}


static void test_directory_path_contracts(SvcObj* fs) {
    fs_command(fs, 2, "/", 0, RC(2, 2));
    fs_command(fs, 2, "/./", 0, RC(2, 2));
    fs_command(fs, 2, "/absent/..", 0, RC(2, 2));
    fs_command(fs, 3, "/", 0, RC(2, 6031));
    fs_command(fs, 4, "/./", 0, RC(2, 6031));

    rename_object(fs, 6, "/absent", "/absent/child", RC(2, 6032));
    rename_object(fs, 6, "/absent/child", "/absent", RC(2, 6032));
    rename_object(fs, 6, "/", "/absent", RC(2, 6032));
    rename_object(fs, 6, "/absent", "/", RC(2, 6032));

    rename_object(fs, 6, "/absent", "/absent", RC(2, 1));
    rename_object(fs, 6, "/absent", "/absent-sibling", RC(2, 1));
    fs_command(fs, 2, "/ancestry", 0, 0);
    fs_command(fs, 2, "/ancestry/child", 0, 0);
    rename_object(fs, 6, "/ancestry", "/ancestry/child", RC(2, 6032));  
    rename_object(fs, 6, "/ancestry/child", "/ancestry", RC(2, 6032));
    rename_object(fs, 6, "/ancestry", "/ancestry-sibling", 0);
    fs_command(fs, 7, "/ancestry-sibling/child", 0, 0);
    fs_command(fs, 4, "/ancestry-sibling", 0, 0);

    fs_command(fs, 2, "/absent/child", 0, RC(2, 1));
    fs_command(fs, 7, "/absent", 0, RC(2, 1));
    puts("PASS Atmosphere normalized directory roots and bidirectional strict ancestry");
}

static void test_directory_services(SvcObj* fs) {
    fs_command(fs, 2, "/list", 0, 0);
    fs_command(fs, 0, "/list/a", 1, 0);
    fs_command(fs, 0, "/list/b", 2, 0);
    fs_command(fs, 0, "/list/c", 3, 0);
    SvcObj* dir = open_object(fs, 9, "/list", 3);
    CHECK(dir_count(dir) == 3);
    CHECK(dir_read(dir, 1) == 1 && !strcmp((char*)GP(0x5000), "a"));
    fs_command(fs, 1, "/list/b", 0, 0);
    fs_command(fs, 0, "/list/aa", 4, 0);
    rename_object(fs, 6, "/list", "/listed", 0);
    CHECK(dir_count(dir) == 3);
    CHECK(dir_read(dir, 2) == 2);
    CHECK(!strcmp((char*)GP(0x5000), "aa"));
    CHECK(!strcmp((char*)GP(0x5310), "c"));
    CHECK(dir_read(dir, 1) == 0);
    fs_command(fs, 0, "/listed/d", 5, 0);
    CHECK(dir_count(dir) == 4);
    CHECK(dir_read(dir, 1) == 0);
    SvcObj* fresh = open_object(fs, 9, "/listed", 2);
    CHECK(dir_count(fresh) == 4 && dir_read(fresh, 8) == 4);
    svcobj_unref(fresh);
    fs_command(fs, 3, "/listed", 0, FS_DIR_NOT_EMPTY);
    fs_command(fs, 13, "/listed", 0, 0);
    CHECK(dir_count(dir) == 0);
    fs_command(fs, 3, "/listed", 0, 0);
    Request removed;
    request(&removed, 1);
    dispatch(dir, &removed, FS_NOTFOUND);
    request(&removed, 0);
    output_buffer(&removed, 0x310);
    dispatch(dir, &removed, FS_NOTFOUND);
    svcobj_unref(dir);
    fs_command(fs, 7, "/listed", 0, FS_NOTFOUND);
    puts("PASS HLE directory cursor/count after create, delete, rename and clean");
}

static void test_index_and_paths(SvcObj* fs) {
    char path[FS_TREE_PATH_MAX];
    CHECK(snprintf(path, sizeof path, "%ssdmc/external.bin", g_datadir) < (int)sizeof path);
    FILE* f = fopen(path, "wb");
    CHECK(f != NULL && fwrite("unindexed", 1, 9, f) == 9 && !fclose(f));
    fs_command(fs, 7, "/external.bin", 0, FS_NOTFOUND);
    fs_command(fs, 1, "/external.bin", 0, FS_NOTFOUND);
    Request r;
    request(&r, 8);
    argument32(&r, 0, 1);
    argument_path(&r, 0, "/external.bin");
    dispatch(fs, &r, FS_NOTFOUND);
    CHECK(r.c.nout_objs == 0);
    u64 size = 123;
    CHECK(fs_open_read("sdmc/external.bin", &size) == NULL && size == 0);
    CHECK(fs_read_file("sdmc/external.bin", &size) == NULL && size == 0);
    PFileMap map = {0};
    CHECK(fs_map_file("sdmc/external.bin", &map) != 0);
    fs_command(fs, 7, "/../root/root.txt", 0, FS_DIR_UNOBTAINABLE);
    fs_command(fs, 7, "romfs:/asset.bin", 0, FS_DIR_UNOBTAINABLE);

    request(&r, 7);
    argument_path(&r, 0, "/seed.bin");
    r.c.x[0].size--;
    u32 result = (u32)fs->dispatch(fs, &r.c);
    CHECK(result != 0 && r.c.out_size == 0);
    request(&r, 7);
    r.c.x[0] = (IpcBuf){0x1000, 0x301, 0};
    r.c.nx = 1;
    memset(GP(0x1000), 'x', 0x301);
    result = (u32)fs->dispatch(fs, &r.c);
    CHECK(result != 0 && r.c.out_size == 0);
    size = 0;
    u8* bytes = fs_read_file("sdmc:/seed.bin", &size);
    CHECK(bytes != NULL && size == 4 && !memcmp(bytes, "seed", 4));
    free(bytes);
    CHECK(!fs_map_file("sdmc/seed.bin", &map));
    CHECK(map.size == 4 && !memcmp(map.data, "seed", 4));
    plat_file_unmap(&map);
    CHECK(snprintf(path, sizeof path, "%ssdmc/seed.bin", g_datadir) < (int)sizeof path);
    f = fopen(path, "ab");
    CHECK(f != NULL && fwrite("unindexed extension", 1, 19, f) == 19 && !fclose(f));
    CHECK(!fs_map_file("sdmc/seed.bin", &map));
    CHECK(map.size == 4 && !memcmp(map.data, "seed", 4));
    plat_file_unmap(&map);
    puts("PASS HLE index-only visibility, confined paths, bounded IPC paths and loader helpers");
}

static void test_storage_contracts(SvcObj* storage, u64 size) {
    const struct {
        u64 off, size, capacity;
        u32 read_result, write_result;
    } cases[] = {
        {UINT64_MAX, UINT64_MAX, 4, RC(2, 6061), RC(2, 6061)},
        {UINT64_MAX, 0, 0, RC(2, 6061), RC(2, 6061)},
        {0, UINT64_MAX, 4, RC(2, 6062), RC(2, 6062)},
        {0, 5, 4, RC(2, 6062), RC(2, 6062)},
        {size + 1, 5, 4, RC(2, 6062), RC(2, 6062)},
        {INT64_MAX, 1, 4, RC(2, 3005), RC(2, 6300)},
        {size, 1, 4, RC(2, 3005), RC(2, 6300)},
        {size - 1, 2, 4, RC(2, 3005), RC(2, 6300)},  
        {size + 1, 1, 4, RC(2, 3005), RC(2, 6300)},
        {size, 0, 0, 0, RC(2, 6300)},
        {INT64_MAX, 0, 0, 0, RC(2, 6300)},
    };
    CHECK(size > 0);
    for (u32 cmd = 0; cmd < 2; ++cmd)
        for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
            Request r;
            request(&r, cmd);
            argument64(&r, 0, cases[i].off);
            argument64(&r, 8, cases[i].size);
            output_buffer(&r, 16);
            if (cmd) {  
                r.c.a[0] = (IpcBuf){0x3000, cases[i].capacity, 0};
                r.c.na = 1;
                r.c.nb = 0;
            } else
                r.c.b[0].size = cases[i].capacity;
            dispatch(storage, &r, cmd ? cases[i].write_result : cases[i].read_result);
            CHECK(r.c.out_size == 0 && r.c.nout_objs == 0);
            for (unsigned j = 0; j < 16; ++j) CHECK(GP(0x5000)[j] == 0xcc);
            CHECK(get_size(storage) == size);
        }
    Request r;
    request(&r, 2);
    dispatch(storage, &r, 0);  
    CHECK(r.c.out_size == 0);
    set_size(storage, UINT64_MAX, RC(2, 6062));
    set_size(storage, 0, RC(2, 6300));
    CHECK(get_size(storage) == size);
}

static void test_virtual_services(SvcObj* fs, SvcObj* proxy) {
    u8 bytes[] = {1, 0, 3, 0xff, 5};
    CHECK(!fs_add_virtual_file("sdmc/virtual.bin", bytes, sizeof bytes));
    SvcObj* file = open_object(fs, 8, "/virtual.bin", 1);
    CHECK(get_size(file) == sizeof bytes);
    CHECK(read_data(file, 1, 10, 0) == 4 && !memcmp(GP(0x5000), bytes + 1, 4));
    write_data(file, 0, "x", 1, FS_WRITE_NOT_PERMITTED);
    set_size(file, 0, FS_WRITE_NOT_PERMITTED);
    Request r;
    request(&r, 8);
    argument32(&r, 0, 3);
    argument_path(&r, 0, "/virtual.bin");
    dispatch(fs, &r, FS_WRITE_NOT_PERMITTED);
    fs_command(fs, 1, "/virtual.bin", 0, FS_WRITE_NOT_PERMITTED);
    svcobj_unref(file);

    request(&r, 202);
    argument64(&r, 8, 0x0100000000000819ull);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    SvcObj* archive = r.c.out_objs[0];
    CHECK(get_size(archive) == 0x200);
    test_storage_contracts(archive, 0x200);
    request(&r, 0);
    argument64(&r, 0, 0);
    argument64(&r, 8, 0x200);
    output_buffer(&r, 0x200);
    dispatch(archive, &r, 0);
    u64 header_size;
    memcpy(&header_size, GP(0x5000), sizeof header_size);
    CHECK(header_size == 0x50);
    request(&r, 1);  
    argument64(&r, 8, 1);
    r.c.a[0] = (IpcBuf){0x3000, 1, 0};
    r.c.na = 1;
    dispatch(archive, &r, RC(2, 6300));
    svcobj_unref(archive);
    puts("PASS HLE readonly virtual files and synthetic system archive");
}

static void test_extracted_romfs(SvcObj* proxy) {
    Request r;
    request(&r, 200);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    SvcObj* storage = r.c.out_objs[0];
    u64 image_size = get_size(storage);
    CHECK(image_size < 0x8000 && image_size > 0x200);
    test_storage_contracts(storage, image_size);
    request(&r, 0);
    argument64(&r, 8, image_size);
    output_buffer(&r, image_size);
    dispatch(storage, &r, 0);
    u8* image = GP(0x5000);
    u64 header[10];
    memcpy(header, image, sizeof header);
    CHECK(header[0] == 0x50 && header[3] < image_size && header[7] < image_size && header[9] < image_size);
    u32 entry;
    memcpy(&entry, image + header[3] + 12, sizeof entry);
    u64 data_offsets[3] = {0}, sizes[3] = {0};
    unsigned found = 0;
    while (entry != UINT32_MAX) {
        CHECK((u64)entry + 0x20 <= header[8]);
        const u8* file = image + header[7] + entry;
        u32 length;
        memcpy(&length, file + 0x1c, sizeof length);
        CHECK((u64)entry + 0x20 + length <= header[8]);
        int which = -1;
        if (length == 9 && !memcmp(file + 0x20, "asset.bin", 9)) which = 0;
        if (length == 9 && !memcmp(file + 0x20, "empty.bin", 9)) which = 1;
        if (length == 8 && !memcmp(file + 0x20, "zulu.bin", 8)) which = 2;
        CHECK(which >= 0 && !(found & (1u << which)));
        memcpy(&data_offsets[which], file + 8, 8);
        memcpy(&sizes[which], file + 16, 8);
        found |= 1u << which;
        memcpy(&entry, file + 4, sizeof entry);
    }
    CHECK(found == 7 && sizes[0] == 10 && sizes[1] == 0 && sizes[2] == 4);
    CHECK(data_offsets[0] == 0 && data_offsets[1] == data_offsets[2] && data_offsets[2] == 16);
    CHECK(!memcmp(image + header[9], "romfs test", 10));
    for (u64 i = 10; i < 16; i++) CHECK(image[header[9] + i] == 0);
    CHECK(!memcmp(image + header[9] + data_offsets[2], "tail", 4));
    request(&r, 0);
    argument64(&r, 0, header[9] + 7);
    argument64(&r, 8, 13);
    output_buffer(&r, 13);
    dispatch(storage, &r, 0);
    const u8 spanning[13] = {'e', 's', 't', 0, 0, 0, 0, 0, 0, 't', 'a', 'i', 'l'};
    CHECK(!memcmp(GP(0x5000), spanning, sizeof spanning));
    svcobj_unref(storage);

    FsNode *asset, *zulu;
    pm_lock(&fsp.tree.lock);
    CHECK(!fs_tree_lookup(&fsp.tree, NULL, "romfs/asset.bin", &asset) && asset->fd >= 0 && asset->readonly);
    CHECK(!fs_tree_lookup(&fsp.tree, NULL, "romfs/zulu.bin", &zulu) && zulu->fd < 0 && zulu->readonly);
    CHECK(fs_tree_remove(&fsp.tree, NULL, "romfs/asset.bin", 0, 0, 0) == FS_TREE_READONLY);
    CHECK(fs_tree_rename(&fsp.tree, NULL, "romfs/asset.bin", "romfs/moved.bin", 0) == FS_TREE_READONLY);
    CHECK(fs_tree_create(&fsp.tree, NULL, "romfs/new.bin", 0, 0, NULL) == FS_TREE_READONLY);
    pm_unlock(&fsp.tree.lock);
    PFileMap map;
    CHECK(!fs_map_file("romfs/asset.bin", &map) && map.borrowed && map.data == asset->map && map.size == 10);
    plat_file_unmap(&map);
    CHECK(!map.data && !map.borrowed && asset->map != NULL);
    u64 size = 0;
    u8* bytes = fs_read_file("romfs/asset.bin", &size);
    CHECK(bytes != NULL && size == 10 && !memcmp(bytes, "romfs test", 10));
    free(bytes);
    puts("PASS extracted RomFS metadata, padding, cross-file reads and zero-length shared offsets");
}

static void test_raw_romfs(SvcObj* proxy, int translated) {
    u8 expected[] = {'R', 'A', 'W', 0, 'R', 'O', 'M', 'F', 'S', 0xff, ' ', 'b', 'y', 't', 'e', 's'};
    if (translated) memcpy(expected, "ESP", 3);
    Request r;
    request(&r, 200);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    SvcObj* storage = r.c.out_objs[0];
    CHECK(fsp.rimage != NULL && fsp.rraw == NULL);  
    CHECK(get_size(storage) == sizeof expected);
    test_storage_contracts(storage, sizeof expected);
    request(&r, 0);
    argument64(&r, 0, 2);
    argument64(&r, 8, sizeof expected - 2);
    output_buffer(&r, sizeof expected);
    dispatch(storage, &r, 0);
    CHECK(!memcmp(GP(0x5000), expected + 2, sizeof expected - 2));
    CHECK(GP(0x5000)[sizeof expected - 2] == 0xcc);
    svcobj_unref(storage);

    request(&r, 205);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    storage = r.c.out_objs[0];
    CHECK(get_size(storage) == sizeof expected);
    svcobj_unref(storage);
    request(&r, 205);
    r.raw[0] = 1;
    dispatch(proxy, &r, RC(2, 1002));
    CHECK(r.c.nout_objs == 0);

    request(&r, 201);
    argument64(&r, 0, g_title_id);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1 && get_size(r.c.out_objs[0]) == sizeof expected);
    svcobj_unref(r.c.out_objs[0]);
    request(&r, 201);
    argument64(&r, 0, g_title_id + 1);
    dispatch(proxy, &r, RC(2, 1002));
    CHECK(r.c.nout_objs == 0);
    puts("PASS raw RomFS reads, cached image size and the storage by program index and id");
}


static SvcObj* open_save_data(SvcObj* proxy, u32 command, u8 type) {
    Request r;
    request(&r, command);
    r.raw[0] = 1;                          
    memset(r.raw + 8, 4, 0x20);            
    memset(r.raw + 0x29, 2, 0x48 - 0x29);  
    r.raw[0x28] = type;
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1 && r.c.out_size == 0);
    return r.c.out_objs[0];
}
static int tree_has(const char* path) {
    FsNode* node;
    pm_lock(&fsp.tree.lock);
    int found = !fs_tree_lookup(&fsp.tree, NULL, path, &node);
    pm_unlock(&fsp.tree.lock);
    return found;
}

static void test_notifiers_and_program_index(SvcObj* proxy) {
    static const u32 notifiers[] = {500, 501, 510};
    Request r;
    for (unsigned i = 0; i < sizeof notifiers / sizeof notifiers[0]; i++) {
        request(&r, notifiers[i]);
        dispatch(proxy, &r, 0);
        CHECK(r.c.nout_objs == 1 && test_events == 1);
        SvcObj* notifier = r.c.out_objs[0];
        u32 handles[2];
        for (int k = 0; k < 2; k++) {
            request(&r, 0);
            dispatch(notifier, &r, 0);
            CHECK(r.c.nout_copy == 1);
            handles[k] = r.c.out_copy[0];
        }
        KEvent* event = test_handles[handles[0] - 0x100];
        CHECK(test_handles[handles[1] - 0x100] == event && !event->signaled);
        kobj_unref(&event->o);  
        kobj_unref(&event->o);
        svcobj_unref(notifier);
        CHECK(test_events == 0);
    }
    request(&r, 83);
    argument64(&r, 0, g_title_id);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    svcobj_unref(r.c.out_objs[0]);
    request(&r, 204);
    dispatch(proxy, &r, RC(2, 1001));
    CHECK(r.c.nout_objs == 0);
    request(&r, 204);
    r.raw[0] = 1;
    dispatch(proxy, &r, RC(2, 1002));
    puts("PASS event notifiers, the save data transfer prohibiter and the data filesystem by program index");
}

static void test_save_data(SvcObj* proxy) {

    SvcObj* system = open_save_data(proxy, 51, 0);
    CHECK(tree_has("save/system") && !tree_has("save/user"));
    SvcObj* bcat = open_save_data(proxy, 51, 2);
    SvcObj* device = open_save_data(proxy, 51, 3);
    CHECK(tree_has("save/bcat") && tree_has("save/device") && !tree_has("save/temp"));
    fs_command(system, 0, "/system.bin", 4, 0);
    fs_command(bcat, 7, "/system.bin", 0, FS_NOTFOUND);
    fs_command(device, 7, "/system.bin", 0, FS_NOTFOUND);
    CHECK(tree_has("save/system/system.bin"));
    fs_command(system, 1, "/system.bin", 0, 0);
    svcobj_unref(bcat);
    svcobj_unref(device);


    SvcObj* user = open_save_data(proxy, 51, 1);
    CHECK(tree_has("save/user") && tree_has("save/system"));
    svcobj_unref(user);
    pm_lock(&fsp.tree.lock);
    CHECK(!fs_tree_remove(&fsp.tree, NULL, "save/user", 1, 1, 0));
    pm_unlock(&fsp.tree.lock);

    fs_command(system, 0, "/slot.bin", 4, 0);
    svcobj_unref(system);
    user = open_save_data(proxy, 51, 1);
    CHECK(tree_has("save/user/slot.bin") && !tree_has("save/system"));
    fs_command(user, 7, "/slot.bin", 0, 0);
    svcobj_unref(user);

    system = open_save_data(proxy, 51, 0);
    fs_command(system, 7, "/slot.bin", 0, FS_NOTFOUND);
    fs_command(system, 0, "/other.bin", 4, 0);
    user = open_save_data(proxy, 51, 1);
    fs_command(user, 7, "/slot.bin", 0, 0);
    fs_command(user, 7, "/other.bin", 0, FS_NOTFOUND);
    CHECK(tree_has("save/system/other.bin"));
    svcobj_unref(user);

    SvcObj* unknown = open_save_data(proxy, 51, 0x7F);
    fs_command(unknown, 7, "/other.bin", 0, 0);
    svcobj_unref(unknown);
    SvcObj* by_id = open_save_data(proxy, 52, 0);
    fs_command(by_id, 7, "/other.bin", 0, 0);
    svcobj_unref(by_id);
    fs_command(system, 1, "/other.bin", 0, 0);
    svcobj_unref(system);


    Request r;
    request(&r, 631);
    dispatch(proxy, &r, 0);
    u32 v;
    CHECK(r.c.out_size == 4);
    memcpy(&v, r.c.out, 4);
    CHECK(v == 1);

    request(&r, 1011);
    dispatch(proxy, &r, 0);
    CHECK(r.c.out_size == 8);
    memcpy(&v, r.c.out, 4);
    CHECK(v == 0);
    memcpy(&v, r.c.out + 4, 4);
    CHECK(v == 1);
    puts("PASS SaveDataType folders, the save/system migration, IsSdCardAccessible and the access log's program index");
}

static void cleanup_service(void) {
    pm_lock(&fsp.tree.lock);
    for (unsigned i = 0; i < NOPEN; i++)
        if (fsp.openf[i].f) fclose(fsp.openf[i].f);
    if (fsp.rraw) fclose(fsp.rraw);
    for (int i = 0; i < fsp.ndn; i++) free(fsp.dn[i].name);
    for (int i = 0; i < fsp.nfn; i++) {
        free(fsp.fn[i].name);
        fs_tree_release(fsp.fn[i].node);
    }
    free(fsp.rmeta);
    free(fsp.rfiles);
    free(fsp.dn);
    free(fsp.fn);
    fs_tree_destroy(&fsp.tree);
    if (fsp.mods_up) fs_tree_destroy(&fsp.mods);
    pm_unlock(&fsp.tree.lock);
    CHECK(test_shared == 0);
    memset(&fsp, 0, sizeof fsp);
}

static void test_mod_overlay(const char* scratch) {

    FsNode image = {0};
    image.size = fsp.rtotal;
    u8* base = malloc((size_t)image.size);
    CHECK(base != NULL && !romfs_read(0, base, image.size));
    image.map = base;
    for (int i = 0; i < NOPEN; i++) {
        if (fsp.openf[i].f) fclose(fsp.openf[i].f);
        memset(&fsp.openf[i], 0, sizeof fsp.openf[i]);
    }
    for (int i = 0; i < fsp.nfn; i++) { free(fsp.fn[i].name); fs_tree_release(fsp.fn[i].node); }
    for (int i = 0; i < fsp.ndn; i++) free(fsp.dn[i].name);
    free(fsp.fn); free(fsp.dn); free(fsp.rfiles); free(fsp.rmeta);
    fsp.fn = NULL; fsp.dn = NULL; fsp.rfiles = NULL; fsp.rmeta = NULL;
    fsp.nfn = fsp.capfn = fsp.ndn = fsp.capdn = fsp.nrfiles = 0;
    fsp.rimage = &image;
    snprintf(g_exedir, sizeof g_exedir, "%s/overlay-fixture/", scratch);
    test_scans_allowed = 1;
    romfs_mods();
    test_scans_allowed = 0;
    CHECK(fsp.nfn == 4 && fsp.ndn == 2 && fsp.mods_up);
    int seen = 0;
    for (int i = 0; i < fsp.nfn; i++) {
        u8 data[32] = {0};
        CHECK(fsp.rfiles[i].size <= sizeof data);
        CHECK(!romfs_read(fsp.rfiles[i].off, data, fsp.rfiles[i].size));
        if (!strcmp(fsp.fn[i].name, "asset.bin")) {
            CHECK(fsp.fn[i].size == 16 && !memcmp(data, "highest priority", 16)); seen |= 1;
        } else if (!strcmp(fsp.fn[i].name, "empty.bin")) {
            CHECK(!fsp.fn[i].size); seen |= 2;
        } else if (fsp.fn[i].parent == 0) {
            CHECK(fsp.fn[i].size == 4 && !memcmp(data, "tail", 4) && !fsp.fn[i].node); seen |= 4;
        } else {
            CHECK(!strcmp(fsp.dn[fsp.fn[i].parent].name, "new"));
            CHECK(fsp.fn[i].size == 12 && !memcmp(data, "unmapped mod", 12));
            CHECK(fsp.fn[i].node->fd < 0 && fsp.fn[i].owner == &fsp.mods); seen |= 8;
        }
    }
    CHECK(seen == 15);
    u8* complete = malloc((size_t)fsp.rtotal);
    CHECK(complete != NULL && !romfs_read(0, complete, fsp.rtotal));
    u64 header[10]; memcpy(header, complete, sizeof header);
    CHECK(header[0] == 0x50 && header[9] == fsp.rdata_off);

    CHECK(!memcmp(base + image.size - 4, "tail", 4));
    free(complete);
    fsp.rimage = NULL;
    free(base);
    puts("PASS layered RomFS priority, size changes, added directories, borrowed base and unmapped mod reads");
}

static void test_addon_images(SvcObj* proxy) {
    u32 indices[4] = {0, 0, 0, 0xdeadbeef};
    const u8 virtual_image[80] = {0x50};
    CHECK(!fs_add_virtual_file("system/archives/0100000000011007.romfs", virtual_image, sizeof virtual_image));
    CHECK(fs_addon_indices(indices, 3) == 3);
    CHECK(indices[0] == 1 && indices[1] == 3 && indices[2] == 10 && indices[3] == 0xdeadbeef);
    indices[2] = 0xdeadbeef;
    CHECK(fs_addon_indices(indices, 2) == 2 && indices[2] == 0xdeadbeef);
    CHECK(fs_addon_indices(NULL, 0) == 0);
    u64 original = g_title_id;
    g_title_id = 0x0100000000030000ull;
    CHECK(fs_addon_indices(indices, 3) == 0);
    g_title_id = original;
    Request r;
    request(&r, 202);
    argument64(&r, 8, g_title_id + 0x1001);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1 && get_size(r.c.out_objs[0]) == 80);
    svcobj_unref(r.c.out_objs[0]);
    request(&r, 202);
    argument64(&r, 8, g_title_id + 0x1002);
    dispatch(proxy, &r, FS_NOTFOUND);
    CHECK(r.c.nout_objs == 0);
    CHECK(fs_addon_indices(indices, 3) == 3);
    puts("PASS DLC image filtering, ordering, capacity, title isolation, mounting and missing-image errors");
}

int main(int argc, char** argv) {
    CHECK((argc == 3 || argc == 4) && strstr(argv[1], "YKW4Recomp-fs-tree-") != NULL);
    CHECK(!test_chdir(argv[1]));
    FILE* marker = fopen(".snx-fs-test", "rb");
    char token[64] = {0};
    CHECK(marker != NULL);
    CHECK(fread(token, 1, sizeof token - 1, marker) == strlen(argv[2]));
    CHECK(!fclose(marker) && !strcmp(token, argv[2]));
    g_mem = calloc(1, 0x10000);
    CHECK(g_mem != NULL);
    int translated = argc == 4 && !strcmp(argv[3], "spanish");
#ifdef _WIN32
    CHECK(!_putenv_s("YKW4_LANGUAGE", translated ? "es" : "original"));
#else
    CHECK(!setenv("YKW4_LANGUAGE", translated ? "es" : "original", 1));
#endif
    int raw = translated || (argc == 4 && !strcmp(argv[3], "raw"));
    strcpy(g_datadir, raw ? "raw-data/" : "service-data/");
    fs_init();
    CHECK(test_scans > 0);
    test_scans_allowed = 0;
    svc_fs_init();
    romfs_init();
    CHECK(!test_chdir(raw ? "raw-data" : "service-data/romfs"));
    SvcObj* proxy = new_fsp();
    if (raw) {
        test_raw_romfs(proxy, translated);
        svcobj_unref(proxy);
        CHECK(test_objects == 0);
        cleanup_service();
        free(g_mem);
        CHECK(!test_chdir(argv[1]));
        printf("PASS %u raw RomFS checks; %u startup scans\n", test_checks, test_scans);
        return 0;
    }
    Request r;
    request(&r, 18);
    dispatch(proxy, &r, 0);
    CHECK(r.c.nout_objs == 1);
    SvcObj* fs = r.c.out_objs[0];
    test_file_services(fs);
    test_file_contracts(fs);
    test_directory_path_contracts(fs);
    test_directory_services(fs);
    test_index_and_paths(fs);
    test_virtual_services(fs, proxy);
    test_extracted_romfs(proxy);
    test_save_data(proxy);
    test_notifiers_and_program_index(proxy);
    test_addon_images(proxy);
    test_mod_overlay(argv[1]);
    svcobj_unref(fs);
    svcobj_unref(proxy);
    CHECK(test_objects == 0);
    cleanup_service();
    free(g_mem);
    CHECK(!test_chdir(argv[1]));
    printf("PASS %u HLE checks; %u startup scans, none after initialization\n", test_checks, test_scans);
    test_scans_allowed = 1;
    CHECK(!fs_pack_romfs_folder("pack-data", "packed.romfs", NULL));
    FILE* packed = fopen("packed.romfs", "rb");
    CHECK(packed != NULL);
    u64 header[10];
    CHECK(fread(header, 1, sizeof header, packed) == sizeof header && header[0] == 0x50);
    u8 payload[36];
    CHECK(plat_pread(packed, payload, sizeof payload, header[9]) == sizeof payload);
    CHECK(!memcmp(payload, "child", 5) && !memcmp(payload + 16, "base", 4) && !memcmp(payload + 32, "tail", 4));
    CHECK(!fclose(packed));
    CHECK(!fsp.tree.root && !test_shared);
    CHECK(!fs_pack_romfs_directory("pack-data/romfs", "direct.romfs", NULL, NULL, NULL));
    FILE* original_image = fopen("packed.romfs", "rb");
    FILE* direct_image = fopen("direct.romfs", "rb");
    CHECK(original_image && direct_image);
    for (;;) {
        unsigned char a[4096], b[4096];
        size_t na = fread(a, 1, sizeof a, original_image), nb = fread(b, 1, sizeof b, direct_image);
        CHECK(na == nb && !memcmp(a, b, na));
        if (!na) { CHECK(!ferror(original_image) && !ferror(direct_image)); break; }
    }
    CHECK(!fclose(original_image) && !fclose(direct_image));
    CHECK(!fsp.tree.root && !test_shared);
    volatile int canceled = 1;
    CHECK(fs_pack_romfs_folder("pack-data", "canceled.romfs", &canceled) != 0);
    CHECK(fopen("canceled.romfs", "rb") == NULL);
    CHECK(fs_pack_romfs_directory("pack-data/romfs", "direct-canceled.romfs", &canceled, NULL, NULL) != 0);
    CHECK(fopen("direct-canceled.romfs", "rb") == NULL);
    puts("PASS first-run RomFS streaming, nested folders, source preservation and cancellation cleanup");
    return 0;
}
