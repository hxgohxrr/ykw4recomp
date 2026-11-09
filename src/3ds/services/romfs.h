/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_ROMFS_H
#define SNX_CTR_ROMFS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common/filesystem/tree.h"
typedef struct SnxCtrRomfs SnxCtrRomfs;

bool snx_ctr_romfs_open(FsTree* tree, SnxCtrRomfs** out);
void snx_ctr_romfs_free(SnxCtrRomfs* fs);
uint64_t snx_ctr_romfs_size(const SnxCtrRomfs* fs);
bool snx_ctr_romfs_read(SnxCtrRomfs* fs, uint64_t offset, void* data, size_t size);

typedef enum { SNX_CTR_TIME_OK = 0, SNX_CTR_TIME_NOTFOUND, SNX_CTR_TIME_INVALID } SnxCtrTimeResult;
SnxCtrTimeResult snx_ctr_romfs_time(SnxCtrRomfs* fs, const unsigned char* path16, size_t bytes, uint64_t* datetime);

bool snx_ctr_utf16_path(const unsigned char* path16, size_t bytes, char* out, size_t out_size);
#endif
