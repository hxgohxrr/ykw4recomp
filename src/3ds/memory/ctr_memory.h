/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_MEMORY_H
#define SNX_CTR_MEMORY_H
#include "3ds/board/board.h"
#define SNX_CTR_PAGE_SIZE 4096u
#define SNX_CTR_READ      1u
#define SNX_CTR_WRITE     2u
#define SNX_CTR_EXEC      4u

typedef enum {
    SNX_CTR_MEM_OK,
    SNX_CTR_MEM_INVALID,
    SNX_CTR_MEM_NOMEM,
    SNX_CTR_MEM_UNMAPPED,
    SNX_CTR_MEM_PERMISSION,
    SNX_CTR_MEM_OVERLAP,
    SNX_CTR_MEM_DEVICE
} SnxCtrMemResult;
typedef struct SnxCtrMemory SnxCtrMemory;
typedef struct SnxCtrAddressSpace SnxCtrAddressSpace;

typedef bool (*SnxCtrMmioRead)(void* context, uint32_t physical, void* bytes, size_t size);
typedef bool (*SnxCtrMmioWrite)(void* context, uint32_t physical, const void* bytes, size_t size);
SnxCtrMemory* snx_ctr_memory_new(SnxCtrModel model);
const SnxCtrProfile* snx_ctr_memory_profile(const SnxCtrMemory* memory);

void snx_ctr_memory_free(SnxCtrMemory* memory);
void snx_ctr_memory_mmio(SnxCtrMemory* memory, void* context, SnxCtrMmioRead read, SnxCtrMmioWrite write);
SnxCtrAddressSpace* snx_ctr_space_new(SnxCtrMemory* memory);
void snx_ctr_space_free(SnxCtrAddressSpace* space);

SnxCtrMemResult snx_ctr_map(SnxCtrAddressSpace* space, uint32_t va, uint32_t pa, uint32_t size, unsigned permissions);
SnxCtrMemResult snx_ctr_protect(SnxCtrAddressSpace* space, uint32_t va, uint32_t size, unsigned permissions);
SnxCtrMemResult snx_ctr_unmap(SnxCtrAddressSpace* space, uint32_t va, uint32_t size);
SnxCtrMemResult snx_ctr_query(const SnxCtrAddressSpace* space, uint32_t va, uint32_t* pa, unsigned* permissions);

uint64_t snx_ctr_memory_epoch(const SnxCtrAddressSpace* space);

SnxCtrMemResult snx_ctr_read(SnxCtrAddressSpace* space, uint32_t va, void* output, size_t size);
SnxCtrMemResult snx_ctr_write(SnxCtrAddressSpace* space, uint32_t va, const void* input, size_t size);
SnxCtrMemResult snx_ctr_fetch(SnxCtrAddressSpace* space, uint32_t va, void* output, size_t size);


SnxCtrMemResult snx_ctr_check_ram(SnxCtrAddressSpace* space, uint32_t va, size_t size, unsigned permissions);
SnxCtrMemResult snx_ctr_read_ram(SnxCtrAddressSpace* space, uint32_t va, void* output, size_t size);
SnxCtrMemResult snx_ctr_write_ram(SnxCtrAddressSpace* space, uint32_t va, const void* input, size_t size);

SnxCtrMemResult snx_ctr_phys_read(SnxCtrMemory* memory, uint32_t pa, void* output, size_t size);
SnxCtrMemResult snx_ctr_phys_write(SnxCtrMemory* memory, uint32_t pa, const void* input, size_t size);
#endif
