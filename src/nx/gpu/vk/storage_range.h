/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_VK_STORAGE_RANGE_H
#define SNX_VK_STORAGE_RANGE_H
#include <stdint.h>


static inline uint32_t vk_compute_storage_range(uint64_t address, uint32_t declared) {
    const uint32_t limit = 16u << 20;
    uint64_t size = declared ? declared : 16;
    if (size > limit) size = limit;
    uint64_t offset = address & 255;
    size = ((offset + size + 255) & ~UINT64_C(255)) - offset;
    if (size > limit) size = limit;
    return (uint32_t)((size + 3) & ~UINT64_C(3));
}
#endif
