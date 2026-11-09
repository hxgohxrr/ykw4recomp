/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_VK_STORAGE_LIMITS_H
#define YKW4_VK_STORAGE_LIMITS_H
#include <stdint.h>
static inline int vk_storage_images_supported(int count, uint32_t per_stage, uint32_t per_set) {
    return count >= 0 && (uint32_t)count <= per_stage && (uint32_t)count <= per_set;
}
#endif
