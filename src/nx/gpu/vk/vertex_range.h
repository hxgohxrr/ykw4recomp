/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNX_VERTEX_RANGE_H
#define SNX_VERTEX_RANGE_H
#include <stdint.h>
static inline uint32_t vk_vertex_binding_padding(uint32_t current, uint32_t stride, uint32_t offset,
    uint32_t size) {
    uint32_t required = stride && offset + size > stride ? offset + size - stride : 0;
    return required > current ? required : current;
}

static inline uint64_t vk_vertex_upload_size(uint64_t length, uint32_t stride, uint32_t pad,
    uint32_t first, uint32_t count, int indexed, int instanced) {
    uint64_t full = length + pad;

    if (indexed || instanced || !stride || first) return full;
    uint64_t prefix = ((uint64_t)first + count) * stride + pad;
    return prefix < full ? prefix : full;
}
#endif
