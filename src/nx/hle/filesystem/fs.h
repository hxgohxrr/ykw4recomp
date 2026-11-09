/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_FS_H
#define SNX_FS_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/runtime/platform/plat.h"


void fs_init(void);
FILE* fs_open_read(const char* path, u64* size);
int fs_map_file(const char* path, PFileMap* map);
u8* fs_read_file(const char* path, u64* size);
int fs_add_virtual_file(const char* path, const void* data, u64 size);

u32 fs_addon_indices(u32* indices, u32 capacity);
void romfs_init(void);

int fs_pack_romfs_folder(const char* root, const char* output, const volatile int* cancel);
int fs_pack_romfs_progress(const char* root, const char* output, const volatile int* cancel,
                           void (*progress)(u64 done, u64 total, void* user), void* user);

int fs_pack_romfs_directory(const char* root, const char* output, const volatile int* cancel,
                             void (*progress)(u64, u64, void*), void* user);

#endif
