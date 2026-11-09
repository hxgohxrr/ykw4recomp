/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNX_OCCLUSION_BUDGET_H
#define SNX_OCCLUSION_BUDGET_H
#include <stdint.h>

typedef struct { uint64_t resume; unsigned unrewarded; } VkOccBudget;
static inline int vk_occ_budget_active(const VkOccBudget* b, uint64_t frame) { return frame >= b->resume; }
static inline void vk_occ_budget_record(VkOccBudget* b, uint64_t frame, uint64_t generation,
                                       uint64_t checked, uint64_t hidden) {
    if (generation < b->resume || !checked) return;
    if (hidden) { b->unrewarded = 0; return; }
    if (checked >= 64 || b->unrewarded >= 64 - checked) {
        b->resume = frame > UINT64_MAX - 120 ? UINT64_MAX : frame + 120;
        b->unrewarded = 0;
    } else b->unrewarded += (unsigned)checked;
}
#endif
