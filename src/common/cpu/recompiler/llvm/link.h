/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_RECOMPILER_LLVM_LINK_H
#define SNX_RECOMPILER_LLVM_LINK_H
#include <stddef.h>

typedef struct {
    const unsigned char* data;
    size_t size;
    const char* name;  
} SnxLinkInput;

typedef struct {
    unsigned char* data;  
    size_t size;
} SnxLinkOutput;


int snx_link(const SnxLinkInput* inputs, size_t count, const char* const* exports, size_t export_count,
             const char* soname, SnxLinkOutput* output, char* error, size_t error_size);
int snx_link_elf(const SnxLinkInput* inputs, size_t count, const char* const* exports, size_t export_count,
                 const char* soname, SnxLinkOutput* output, char* error, size_t error_size);
int snx_link_coff(const SnxLinkInput* inputs, size_t count, const char* const* exports, size_t export_count,
                  const char* soname, SnxLinkOutput* output, char* error, size_t error_size);


typedef struct {
    unsigned char* data;
    size_t size, capacity;
} SnxLinkBuffer;
void snx_link_reserve(SnxLinkBuffer* buffer, size_t size);  
size_t snx_link_append(SnxLinkBuffer* buffer, const void* data, size_t size);
void snx_link_align(SnxLinkBuffer* buffer, size_t alignment);

typedef struct {
    char** keys;
    size_t* values;
    size_t count, capacity;
} SnxLinkNames;

int snx_link_find(const SnxLinkNames* names, const char* key, size_t* value);
void snx_link_insert(SnxLinkNames* names, const char* key, size_t value);
void snx_link_names_free(SnxLinkNames* names);

#endif
