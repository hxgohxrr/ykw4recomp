/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_PLAT_H
#define SNX_PLAT_H
#include "common/cpu/aarch64/cpu.h"
#include "common/filesystem/host.h"
#include <stddef.h>
#include <stdio.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  
#endif
#include <windows.h>
typedef struct {
    CONDITION_VARIABLE c;
} PCond;
#else
#include <pthread.h>
typedef struct {
    pthread_cond_t c;
} PCond;
#endif
void pc_init(PCond* c);
void pc_signal(PCond* c);
void pc_broadcast(PCond* c);
int pc_wait(PCond* c, PMutex* m, i64 timeout_ns);  
typedef void (*PThreadFn)(void* arg);
int plat_thread(PThreadFn fn, void* arg, size_t stack);
void plat_thread_exit(void);

void plat_exit_now(int code) __attribute__((noreturn));
void plat_sleep_ns(i64 ns);

int plat_text_input(char* out, size_t cap, const char* initial);

typedef struct PAudio PAudio;
PAudio* plat_audio_open(u32 rate, u32 channels);
void plat_audio_close(PAudio* a);
int plat_audio_queue(PAudio* a, const void* samples, u32 bytes);
void plat_audio_start(PAudio* a, int started);
void plat_audio_clear(PAudio* a);
u32 plat_audio_queued(PAudio* a);  
void plat_audio_volume(PAudio* a, float volume);
void plat_yield(void);
u64 plat_ns(void);  
i64 plat_unix_time(void);
i64 plat_tz_offset(void);  
u8* plat_vm_reserve(u64 size);

int plat_ww_scan(u8* p, u64 size, void (*cb)(u8* p, u64 size));
int plat_vm_commit(u8* p, u64 size);
void plat_vm_decommit(u8* p, u64 size);
void* plat_dl_open(const char* path);
void* plat_dl_sym(void* h, const char* name);
const char* plat_dl_error(void);  
int plat_is_dir(const char* path);
typedef struct {
    void* h;
    int first;
    char name[512];
    int is_dir;
    u64 size;
} PDir;
int plat_dir_open(PDir* d, const char* path);
int plat_dir_next(PDir* d);
void plat_dir_close(PDir* d);
u64 plat_file_size(const char* path);
i64 plat_pread(FILE* f, void* buf, u64 size, u64 off);  

typedef struct {
    const u8* data;
    u64 size;
    int borrowed;  
} PFileMap;
int plat_file_map(PFileMap* m, const char* path);  

int plat_file_map_sized(PFileMap* m, const char* path, u64 size);
void plat_file_unmap(PFileMap* m);


int plat_replace_file(const char* tmp, const char* dst);
void plat_install_crash_handler(void);
extern __thread void* tls_thread;
void plat_prof_start(void);  
#if defined(_WIN32)
void plat_prof_add_thread(HANDLE h);
#endif

#endif
