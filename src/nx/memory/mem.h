/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_MEM_H
#define SNX_MEM_H
#include "common/cpu/aarch64/cpu.h"
#include <string.h>


#define AS_SIZE 0x2000000000ull

typedef struct {
    u64 base, end;  
    u64 heap, heap_size, alias, alias_size, stack, stack_size;
    u64 kern, kern_end;  
} MemLayout;
extern MemLayout g_layout;
#define AS_BASE        (g_layout.base)
#define AS_END         (g_layout.end)
#define HEAP_BASE      (g_layout.heap)
#define HEAP_SIZE      (g_layout.heap_size)
#define ALIAS_BASE     (g_layout.alias)
#define ALIAS_SIZE     (g_layout.alias_size)
#define STACK_BASE     (g_layout.stack)
#define STACK_SIZE     (g_layout.stack_size)
#define KERN_BASE      (g_layout.kern)
#define KERN_END       (g_layout.kern_end)

#define AS32_CODE_BASE 0x200000ull

void mem_layout32(unsigned aspace);

enum {
    MS_FREE = 0x00000000,
    MS_IO = 0x00182001,
    MS_STATIC = 0x00042002,
    MS_CODE = 0x04DC7E03,
    MS_CODEDATA = 0x07FEBD04,
    MS_NORMAL = 0x077EBD05,
    MS_SHARED = 0x04402006,
    MS_ALIASCODE = 0x04DD7E08,
    MS_ALIASCODEDATA = 0x07FFBD09,
    MS_IPC = 0x045C3C0A,
    MS_STACK = 0x045C3C0B,
    MS_TLS = 0x0400000C,
    MS_TRANSFERED = 0x055C3C0D,
    MS_SHAREDTRANSFERED = 0x045C380E,
    MS_SHAREDCODE = 0x0440380F,
    MS_INACCESSIBLE = 0x00000010,
    MS_KERNEL = 0x00000013,
    MS_GENCODE = 0x04402214,
    MS_CODEOUT = 0x04402015
};
enum { PERM_NONE = 0, PERM_R = 1, PERM_W = 2, PERM_RW = 3, PERM_X = 4, PERM_RX = 5, PERM_DONTCARE = 0x10000000 };

extern u8* g_mem;
typedef struct {
    u64 addr, size;
    u32 state, perm, attr, ipc, dev;
} MemBlock;
void mem_init(void);
int mem_map(u64 addr, u64 size, u32 state, u32 perm);  

enum { MEM_BADSTATE = -1, MEM_NOMEM = -2, MEM_LIMIT = -3 };
int mem_map_physical(u64 addr, u64 size, u64 budget);
int mem_unmap_physical(u64 addr, u64 size);
int mem_unmap(u64 addr, u64 size);

typedef struct {
    u64 base, source, size, bss, bss_size;
} MemCodeMap;
int mem_map_code(MemCodeMap* m);  
int mem_unmap_code(const MemCodeMap* m);
void mem_query(u64 addr, MemBlock* out);
int mem_set_state(u64 addr, u64 size, u32 state, u32 perm);
int mem_set_attr(u64 addr, u64 size, u32 mask, u32 attr);
u64 mem_find_free(u64 lo, u64 hi, u64 size, u64 align);
u64 kmem_alloc(u64 size, u32 state, u32 perm);  
int mem_heap_set(u64 size, u64 budget);         
extern u64 g_heap_size, g_phys_mapped;          
extern u64 g_heap_max;                          
extern u64 g_code_memory, g_main_stack_size;    
#define GP(a) (g_mem + (u64)(a))
static inline u32 gr32(u64 a) {
    u32 v;
    memcpy(&v, GP(a), 4);
    return v;
}
static inline u64 gr64(u64 a) {
    u64 v;
    memcpy(&v, GP(a), 8);
    return v;
}
static inline void gw32(u64 a, u32 v) { memcpy(GP(a), &v, 4); }
static inline void gw64(u64 a, u64 v) { memcpy(GP(a), &v, 8); }

#endif
