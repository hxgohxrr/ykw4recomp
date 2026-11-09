/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nx/gpu/vk/vk.h"
#include "nx/runtime/platform/log.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int mode, calls;
void logf_(int level, const char* fmt, ...) { (void)level; (void)fmt; }
static VKAPI_ATTR VkResult VKAPI_CALL read_fault(VkDevice device, VkDeviceFaultCountsEXT* counts, VkDeviceFaultInfoEXT* info) {
    assert(device == (VkDevice)(uintptr_t)1);
    assert(counts->sType == VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);
    ++calls;
    if (mode == 2) return VK_ERROR_UNKNOWN;
    if (!info) {
        counts->addressInfoCount = mode == 1 ? 4096 : mode == 3 ? 0 : 1;
        counts->vendorInfoCount = mode == 1 ? 4096 : mode == 3 ? 0 : 1;
        return VK_SUCCESS;
    }
    assert(info->sType == VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT);
    assert(counts->vendorBinarySize == 0 && !info->pVendorBinaryData);
    if (mode == 4) return VK_ERROR_UNKNOWN;
    if (mode == 1) assert(counts->addressInfoCount == 256 && counts->vendorInfoCount == 256);
    if (counts->addressInfoCount) {
        assert(info->pAddressInfos);
        info->pAddressInfos[0].reportedAddress = 0x12340000;
        counts->addressInfoCount = 1;
    }
    if (counts->vendorInfoCount) {
        assert(info->pVendorInfos);
        strcpy(info->pVendorInfos[0].description, "synthetic fault");
        counts->vendorInfoCount = 1;
    }
    strcpy(info->description, "synthetic device loss; no GPU accessed");
    return mode == 1 ? VK_INCOMPLETE : VK_SUCCESS;
}
int main(void) {
    assert(!vk_device_fault_details(NULL, (VkDevice)(uintptr_t)1));
    assert(!vk_device_fault_details(read_fault, VK_NULL_HANDLE));
    assert(calls == 0);
    for (mode = 0; mode < 5; ++mode) {
        calls = 0;
        assert(vk_device_fault_details(read_fault, (VkDevice)(uintptr_t)1) == (mode != 2 && mode != 4));
        assert(calls == (mode == 2 ? 1 : 2));
    }
    puts("PASS bounded fault reports, partial/empty reports and failed queries; no GPU loss induced");
    return 0;
}
