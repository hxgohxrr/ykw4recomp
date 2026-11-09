/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GNU_PROCESS_H
#define SNX_GNU_PROCESS_H
#include "common/cpu/aarch64/cpu.h"
#include "common/cpu/arm/arm.h"
#include "gnuabi/memory/memory.h"
#include "gnuabi/loader/elf/elf.h"
#include "gnuabi/filesystem/vfs.h"
#include "gnuabi/abi/structures/signal.h"
#include "gnuabi/runtime/jit.h"
#include <setjmp.h>
#include <stdbool.h>

typedef struct GnuExec {
    char* path;             
    unsigned char* image;   
    size_t size;
    char** argv;            
    char** envp;
} GnuExec;
typedef struct GnuProcess {
    CPU cpu;
    SnxArmCPU arm;  
    GnuMemory memory;
    GnuElf elf;
    GnuElf interp;  
    GnuVfs vfs;
    GnuJit jit;
    uint32_t pid, tid;
    uint64_t clear_tid, signal_mask;
    GnuSignalAction actions[65];
    jmp_buf exit_target;
    int running, exited, exit_code, faulted;
    GnuExec* exec;   
    int vfork_fd;    

    struct GnuVfork* vfork_parent;
    struct GnuChild* children;
    size_t child_count;
    char diagnostic[256];
} GnuProcess;
int gnu_random_bytes(void* destination, size_t size);
int gnu_clock_gettime(int clock_id, int64_t* seconds, int64_t* nanoseconds);
void gnu_sleep(int64_t seconds, int64_t nanoseconds);

void gnu_start_sampler(GnuProcess* process);

int gnu_process_init(GnuProcess* process, const char* path, int argc, const char* const* argv,
                     const char* const* environment, const char* sysroot, char* error, size_t error_capacity);

int gnu_process_load(GnuProcess* process, const char* path, const unsigned char* image, size_t size, int argc,
                     const char* const* argv, const char* const* environment, char* error, size_t error_capacity);
int gnu_process_stack(GnuProcess* process, int argc, const char* const* argv, const char* const* environment);
int gnu_process_stack32(GnuProcess* process, int argc, const char* const* argv, const char* const* environment);
void gnu_process_destroy(GnuProcess* process);
void gnu_process_exit(GnuProcess* process, int status);
void gnu_process_fault(GnuProcess* process, int signal, const char* reason);
int64_t gnu_syscall(GnuProcess* process, uint64_t number, const uint64_t arguments[6]);

bool gnu_spawn_syscall(GnuProcess* process, uint64_t number, const uint64_t arguments[6], int64_t* result);

int gnu_spawn_finish_exec(GnuProcess* process);

void gnu_spawn_exit(GnuProcess* process);
#ifdef _WIN32

int64_t gnu_spawn_win32_vfork(GnuProcess* process, uint64_t flags, uint64_t stack, uint64_t parent_tid, uint64_t tls,
                              uint64_t child_tid);
int64_t gnu_spawn_win32_exec(GnuProcess* process, GnuExec* exec);
void gnu_spawn_win32_child_exit(GnuProcess* process, int status);
int64_t gnu_spawn_win32_wait(GnuProcess* process, int32_t pid, uint64_t status, int no_hang, uint64_t usage);
int64_t gnu_spawn_win32_kill(GnuProcess* process, int32_t pid, int signal);
#endif

void gnu_spawn_set_options(int count, const char* const* options);

void gnu_spawn_signal_disposition(int signal, uint64_t handler);
extern const RtAPI gnu_rtapi;
#endif
