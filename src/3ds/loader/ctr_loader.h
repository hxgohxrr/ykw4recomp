/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_LOADER_H
#define SNX_CTR_LOADER_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    SNX_CTR_EXTRACT_OK,
    SNX_CTR_EXTRACT_IO,
    SNX_CTR_EXTRACT_FORMAT,
    SNX_CTR_EXTRACT_UNSUPPORTED,
    SNX_CTR_EXTRACT_LIMIT,
    SNX_CTR_EXTRACT_EXISTS
} SnxCtrExtractResult;

SnxCtrExtractResult snx_ctr_extract(const char* input, const char* destination, const char* work, char** published,
                                    char* error, size_t error_size);

bool snx_ctr_blz(const unsigned char* input, size_t input_size, unsigned char* output, size_t output_size);
#endif
