/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nx/gpu/vk/storage_limits.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    assert(vk_storage_images_supported(0, UINT32_MAX, UINT32_MAX));
    assert(vk_storage_images_supported(8, UINT32_MAX, UINT32_MAX));
    assert(vk_storage_images_supported(8, 8, UINT32_MAX));
    assert(vk_storage_images_supported(8, UINT32_MAX, 8));
    assert(!vk_storage_images_supported(9, 8, UINT32_MAX));
    assert(!vk_storage_images_supported(9, UINT32_MAX, 8));
    assert(vk_storage_images_supported(0, 0, 0));
    assert(!vk_storage_images_supported(1, 0, 0));
    assert(!vk_storage_images_supported(-1, UINT32_MAX, UINT32_MAX));
    puts("Storage image limits preserve unsigned Vulkan ranges.");
    return 0;
}
