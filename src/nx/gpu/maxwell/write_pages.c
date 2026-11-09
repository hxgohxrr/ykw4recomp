/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#include "nx/gpu/maxwell/write_pages.h"
#include <stdlib.h>

#define LEAF_PAGES 4096u
#define GROUP_PAGES 64u
struct WritePageLeaf {
    u64 max;
    u64 group_max[LEAF_PAGES / GROUP_PAGES];
    u64 gen[LEAF_PAGES];
};
void write_pages_stamp(WritePages* t, u64 a, u64 size, u64 gen) {
    u64 p, e;
    if (!size) return;
    if (a + size - 1 < a) {
        __atomic_store_n(&t->far, gen, __ATOMIC_RELEASE);
        return;
    }
    for (p = a >> 12, e = (a + size - 1) >> 12; p <= e; p++) {
        WritePageLeaf** l = &t->leaf[(p >> 12) & 0xFFFF];
        WritePageLeaf* leaf = p >> 28 ? NULL : __atomic_load_n(l, __ATOMIC_ACQUIRE);
        if (!leaf && !(p >> 28) && (leaf = calloc(1, sizeof(WritePageLeaf))))
            __atomic_store_n(l, leaf, __ATOMIC_RELEASE);
        if (!leaf) {
            __atomic_store_n(&t->far, gen, __ATOMIC_RELEASE);
            return;
        }
        __atomic_store_n(&leaf->gen[p & (LEAF_PAGES - 1)], gen, __ATOMIC_RELAXED);
        __atomic_store_n(&leaf->group_max[(p & (LEAF_PAGES - 1)) / GROUP_PAGES], gen, __ATOMIC_RELEASE);
        __atomic_store_n(&leaf->max, gen, __ATOMIC_RELEASE);
    }
}
int write_pages_since(const WritePages* t, u64 since, u64 a, u64 size) {
    u64 p, e;
    if (__atomic_load_n(&t->far, __ATOMIC_ACQUIRE) > since) return 1;
    if (!size) return 0;
    e = a + size - 1 < a ? ~0ull >> 12 : (a + size - 1) >> 12;  
    for (p = a >> 12; p <= e && !(p >> 28);) {
        u64 end = (p | (LEAF_PAGES - 1)) < e ? p | (LEAF_PAGES - 1) : e;
        const WritePageLeaf* l = __atomic_load_n(&t->leaf[p >> 12], __ATOMIC_ACQUIRE);
        if (l && __atomic_load_n(&l->max, __ATOMIC_ACQUIRE) > since)
            for (u64 group = p; group <= end;) {
                u64 group_end = (group | (GROUP_PAGES - 1)) < end ? group | (GROUP_PAGES - 1) : end;
                if (__atomic_load_n(&l->group_max[(group & (LEAF_PAGES - 1)) / GROUP_PAGES], __ATOMIC_ACQUIRE) > since)
                    for (u64 q = group; q <= group_end; q++)
                        if (__atomic_load_n(&l->gen[q & (LEAF_PAGES - 1)], __ATOMIC_RELAXED) > since) return 1;
                group = group_end + 1;
            }
        p = end + 1;
    }
    return 0;
}
