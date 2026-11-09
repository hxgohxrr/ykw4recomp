/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GNU_VFS_H
#define SNX_GNU_VFS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gnuabi/memory/memory.h"

typedef struct GnuVfs {
    struct GnuVfsState* state;
} GnuVfs;
int gnu_vfs_init(GnuVfs* vfs);
void gnu_vfs_destroy(GnuVfs* vfs);
int gnu_vfs_set_process(GnuVfs* vfs, uint32_t pid, const char* exe, int argc, const char* const* argv);
void gnu_vfs_set_pid(GnuVfs* vfs, uint32_t pid);  

int gnu_vfs_set_executable(GnuVfs* vfs, const void* bytes, size_t length);

int gnu_vfs_set_root(GnuVfs* vfs, const char* sysroot);

int gnu_hostfs_program_path(GnuVfs* vfs, const char* host, char* guest, size_t capacity);

void gnu_vfs_close_on_exec(GnuVfs* vfs);

int gnu_vfs_pipe(GnuVfs* vfs, unsigned flags, int fds[2]);

int gnu_vfs_load(GnuVfs* vfs, int dirfd, const char* path, unsigned char** bytes, size_t* size, char* guest,
                 size_t guest_capacity);

int gnu_vfs_host_descriptor(GnuVfs* vfs, int fd);

int gnu_vfs_host_path(GnuVfs* vfs, const char* guest, char* host, size_t capacity);

const char* gnu_vfs_cwd(GnuVfs* vfs);
int gnu_vfs_set_cwd(GnuVfs* vfs, const char* guest);

int gnu_vfs_adopt(GnuVfs* vfs, int fd, int host);

struct GnuVfsTable;
struct GnuVfsTable* gnu_vfs_save_table(GnuVfs* vfs);
void gnu_vfs_restore_table(GnuVfs* vfs, struct GnuVfsTable* table);
bool gnu_vfs_syscall(GnuVfs* vfs, GnuMemory* memory, uint64_t number, const uint64_t args[6], int64_t* result);
int64_t gnu_vfs_pread(GnuVfs* vfs, int fd, void* destination, size_t length, uint64_t offset);


typedef struct GnuObjectOps {
    const char* link;    
    unsigned file_type;  
    int64_t (*read)(void* object, unsigned flags, void* data, size_t size);
    int64_t (*write)(void* object, unsigned flags, const void* data, size_t size);
    int (*readable_bytes)(void* object, int* bytes);  
    void (*release)(void* object);                    
} GnuObjectOps;

int gnu_vfs_install_object(GnuVfs* vfs, const GnuObjectOps* ops, void* object, unsigned flags, int cloexec);

void* gnu_vfs_object(GnuVfs* vfs, int fd, const GnuObjectOps* ops, unsigned* flags);

typedef struct GnuFdInfo {
    const GnuObjectOps* ops;  
    void* object;
    int host_fd;  
    int regular;  
    unsigned flags;
    const void* identity;  
} GnuFdInfo;
int gnu_vfs_fd_info(GnuVfs* vfs, int fd, GnuFdInfo* out);  

int gnu_vfs_add_file(GnuVfs* vfs, const char* path, const void* data, size_t size);
#endif
