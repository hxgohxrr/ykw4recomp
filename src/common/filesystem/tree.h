/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_FS_TREE_H
#define SNX_FS_TREE_H

#include "common/filesystem/host.h"
#include <stdio.h>

#define FS_TREE_PATH_MAX 4096

typedef enum {
    FS_TREE_OK = 0,
    FS_TREE_NOTFOUND,
    FS_TREE_EXISTS,
    FS_TREE_NOTEMPTY,
    FS_TREE_INVALID,
    FS_TREE_TOOLONG,
    FS_TREE_IO,
    FS_TREE_NOMEM,
    FS_TREE_READONLY,
    FS_TREE_NOTDELETABLE,
    FS_TREE_NOTRENAMABLE
} FsTreeResult;

typedef struct FsNode {
    char* name;
    struct FsNode *parent, *children, *next;
    uint64_t size;
    int is_dir, is_virtual, readonly, attached;
    uint8_t* virtual_data;

    int fd;
    const uint8_t* map;
    unsigned refs;  
} FsNode;

typedef struct {
    char* base;    
    FsNode* root;  
    PMutex lock;
} FsTree;


FsTreeResult fs_tree_init(FsTree* tree, const char* directory);

FsTreeResult fs_tree_init_directory(FsTree* tree, const char* directory);
void fs_tree_destroy(FsTree* tree);

FsTreeResult fs_tree_lookup(FsTree* tree, FsNode* scope, const char* path, FsNode** out);
FsTreeResult fs_tree_path(FsTree* tree, const FsNode* node, char* out, size_t capacity);

FILE* fs_tree_open(FsTree* tree, FsNode* node, const char* mode);

FsTreeResult fs_tree_resize(FsTree* tree, FsNode* node, uint64_t size);

FsTreeResult fs_tree_mtime(FsTree* tree, const FsNode* node, uint64_t* unix_ms);
FsTreeResult fs_tree_mkdirs(FsTree* tree, const char* relative_path, FsNode** out);
FsTreeResult fs_tree_create(FsTree* tree, FsNode* scope, const char* path, int is_dir, uint64_t size, FsNode** out);
FsTreeResult fs_tree_remove(FsTree* tree, FsNode* scope, const char* path, int is_dir, int recursive, int keep);
FsTreeResult fs_tree_rename(FsTree* tree, FsNode* scope, const char* src, const char* dst, int is_dir);

FsTreeResult fs_tree_add_virtual(FsTree* tree, const char* relative_path, const void* data, uint64_t size,
                                 FsNode** out);

FsTreeResult fs_tree_share(FsTree* tree, FsNode* node, unsigned* shared, unsigned* failed);
void fs_tree_retain(FsNode* node);
void fs_tree_release(FsNode* node);

#endif
