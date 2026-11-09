/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include "nx/gpu/vk/occlusion_budget.h"
int main(void) {
 VkOccBudget b = {0};
 assert(vk_occ_budget_active(&b, 1));
 vk_occ_budget_record(&b, 5, 1, 32, 0);
 assert(vk_occ_budget_active(&b, 6));
 vk_occ_budget_record(&b, 6, 2, 32, 0);
 assert(!vk_occ_budget_active(&b, 125) && vk_occ_budget_active(&b, 126));
 vk_occ_budget_record(&b, 126, 3, 64, 0);  
 assert(vk_occ_budget_active(&b, 126));
 vk_occ_budget_record(&b, 130, 126, 63, 0);
 vk_occ_budget_record(&b, 131, 127, 1, 1);  
 vk_occ_budget_record(&b, 132, 128, 1, 0);
 assert(vk_occ_budget_active(&b, 133));
 vk_occ_budget_record(&b, 134, 129, 0, 0);assert(b.unrewarded == 1);
 vk_occ_budget_record(&b, 135, 130, UINT64_MAX, 0);assert(!vk_occ_budget_active(&b, 136));
 return 0;
}
