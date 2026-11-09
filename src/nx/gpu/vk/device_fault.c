/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include "nx/gpu/vk/vk.h"
#include "nx/runtime/platform/log.h"

int vk_device_fault_details(PFN_vkGetDeviceFaultInfoEXT read, VkDevice device) {
    if (!read || !device) return 0;
    VkDeviceFaultCountsEXT counts = {.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
    VkResult result = read(device, &counts, NULL);
    if (result != VK_SUCCESS) {
        WARN("vulkan device fault: count query failed (%d)\n", (int)result);
        return 0;
    }
    u32 na = counts.addressInfoCount > 256 ? 256 : counts.addressInfoCount;
    u32 nv = counts.vendorInfoCount > 256 ? 256 : counts.vendorInfoCount;
    VkDeviceFaultAddressInfoEXT* addresses = na ? calloc(na, sizeof *addresses) : NULL;
    VkDeviceFaultVendorInfoEXT* vendors = nv ? calloc(nv, sizeof *vendors) : NULL;
    if (!addresses) na = 0;
    if (!vendors) nv = 0;
    counts.addressInfoCount = na;
    counts.vendorInfoCount = nv;
    counts.vendorBinarySize = 0;
    VkDeviceFaultInfoEXT info = {.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT,
                                 .pAddressInfos = addresses, .pVendorInfos = vendors};
    result = read(device, &counts, &info);
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
        WARN("vulkan device fault: %.255s (result %d)\n", info.description, (int)result);
        for (u32 i = 0; i < counts.addressInfoCount && i < na; ++i)
            WARN("vulkan device fault: address type=%u address=%016llx precision=%llu\n",
                 (unsigned)addresses[i].addressType, (unsigned long long)addresses[i].reportedAddress,
                 (unsigned long long)addresses[i].addressPrecision);
        for (u32 i = 0; i < counts.vendorInfoCount && i < nv; ++i)
            WARN("vulkan device fault: vendor %.255s code=%016llx data=%016llx\n", vendors[i].description,
                 (unsigned long long)vendors[i].vendorFaultCode, (unsigned long long)vendors[i].vendorFaultData);
    } else WARN("vulkan device fault: detail query failed (%d)\n", (int)result);
    free(addresses);
    free(vendors);
    return result == VK_SUCCESS || result == VK_INCOMPLETE;
}
