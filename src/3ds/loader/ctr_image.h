/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_IMAGE_H
#define SNX_CTR_IMAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t address, pages, bytes; } SnxCtrSegment;
typedef struct {
    SnxCtrSegment segment[3];  
    uint32_t bss_size, stack_size;
    uint64_t title_id;

    uint32_t kernel_caps[28];
    unsigned char identity[32];  
    unsigned char* code;
    size_t code_size;
} SnxCtrImage;

bool snx_ctr_image_load(const char* directory, SnxCtrImage* image, char* error, size_t error_size);
void snx_ctr_image_free(SnxCtrImage* image);
#endif
