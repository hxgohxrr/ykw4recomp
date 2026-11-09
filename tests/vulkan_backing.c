/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#include <assert.h>
#include "nx/gpu/vk/backing.h"
#include "nx/gpu/vk/storage_range.h"
#include "nx/gpu/vk/vertex_range.h"
#include "nx/gpu/vk/index_convert.h"
u64 gmmu_translate(u64 a) {
    if (a >= 0x100000 && a < 0x200000) return a - 0xF0000;
    if (a >= 0x300000 && a < 0x400000) return a - 0x2F0000;
    if (a == 0x500000) return 0x900000;
    if (a == 0x501000) return 0x10000;
    return 0;
}
int main(void) {
    for (uint32_t value = 0; value <= UINT8_MAX; value++) {
        assert(vk_index_widen_u8((uint8_t)value, 0, value) == value);
        assert(vk_index_widen_u8((uint8_t)value, 1, UINT8_MAX) ==
            (value == UINT8_MAX ? UINT16_MAX : value));
        assert(vk_index_widen_u8((uint8_t)value, 1, 0x107) ==
            (value == 7 ? UINT16_MAX : value));
    }
    uint32_t pad = vk_vertex_binding_padding(0, 6, 0, 4);
    assert(pad == 0);
    pad = vk_vertex_binding_padding(pad, 6, 0, 8);
    assert(pad == 2);
    assert(vk_vertex_upload_size(60, 6, pad, 0, 10, 0, 0) == 62);
    assert(vk_vertex_upload_size(60, 6, pad, 0, 3, 1, 0) == 62);
    assert(vk_vertex_upload_size(60, 6, pad, 0, 3, 0, 1) == 62);
    assert(vk_vertex_binding_padding(pad, 6, 0, 4) == 2);
    assert(vk_vertex_binding_padding(0, 6, 0, 8) == pad);
    assert(vk_vertex_binding_padding(0, 16, 4096, 4) == 4084);
    assert(vk_vertex_binding_padding(4084, 16, 4096, 16) == 4096);
    assert(vk_vertex_binding_padding(0, 0, 12, 16) == 0);
    assert(vk_vertex_upload_size(1u << 20, 32, 0, 0, 6, 0, 0) == 192);
    assert(vk_vertex_upload_size(1u << 20, 32, 0, 100, 6, 0, 0) == (1u << 20));
    assert(vk_vertex_upload_size(1u << 20, 6, 2, 0, 10, 0, 0) == 62);
    assert(vk_vertex_upload_size(128, 32, 0, 0, 100, 0, 0) == 128);
    assert(vk_vertex_upload_size(4096, 32, 0, 0, 3, 1, 0) == 4096);  
    assert(vk_vertex_upload_size(4096, 32, 0, 0, 3, 0, 1) == 4096);  
    assert(vk_vertex_upload_size(4096, 0, 0, 0, 3, 0, 0) == 4096);  
    assert(vk_vertex_upload_size(4096, 4095, 0, UINT32_MAX, UINT32_MAX, 0, 0) == 4096);

    assert(vk_compute_storage_range(0x5ba630000ull, 4) == 64 * sizeof(float));
    assert(vk_compute_storage_range(0x5ba630100ull, 4) == 256);
    assert(vk_compute_storage_range(0x100004, 4) == 252);
    assert(vk_compute_storage_range(0x1000fc, 4) == 4);
    assert(vk_compute_storage_range(0x1000fc, 8) == 260);
    assert(vk_compute_storage_range(0x100000, 0) == 256);
    assert(vk_compute_storage_range(0x100000, UINT32_MAX) == (16u << 20));
    assert(vk_backing_overlap(0x100000, 16, 0x300000, 16));
    assert(vk_backing_overlap(0x100000, 4096, 0x500000, 8192));
    assert(!vk_backing_overlap(0x100000, 4096, 0x500000, 4096));
    assert(!vk_backing_overlap(0x700000, 4096, 0x800000, 4096));
    assert(!vk_backing_overlap(0x100000, 4096, 0x301000, 4096));
    assert(vk_backing_overlap(0x100000, 65 * 4096, 0x300000, 65 * 4096));
    assert(vk_backing_overlap(0x100FFF, 2, 0x301000, 1));
    assert(!vk_backing_overlap(0x100000, 0, 0x100000, 16));
    return 0;
}
