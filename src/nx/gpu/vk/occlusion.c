/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include <string.h>
#include <stdlib.h>
#include "nx/gpu/vk/vk.h"
#include "nx/gpu/gpu.h"
#include "occlusion_budget.h"

#define OCC_QUERIES 8192
#define OCC_SCOPE 1024
#define OCC_BUDGET VK_OCC_BUDGET  
typedef struct {
    VkQueryPool pool;
    VkBuffer predicates;
    VmaAllocation memory;
    u64 generation;
    u32 used, sampled, finished_count, flushed;
    u32 finished_indices[OCC_BUDGET];
    u32 owner[OCC_QUERIES], gpu_indices[OCC_QUERIES];
    u8 safe32[OCC_QUERIES], copied[OCC_QUERIES], finished[OCC_QUERIES];
    u32 guarded[OCC_BUDGET], inverted[OCC_BUDGET];
} OccPool;
typedef struct {
    u64 generation, gate;
    u32 indices[OCC_SCOPE], count;
    int active, uncertain;
    u64 epoch;
} OccScope;
typedef struct { u64 address, generation, epoch; u32 channel, cursor; } OccMarker;
static OccMarker markers[4096];
static OccPool pools[VK_FRAMES];
static OccScope scopes[128];
static struct { u64 samples, available; } results[OCC_QUERIES];
u64 g_vk_occ_ready, g_vk_occ_fallback, g_vk_occ_queries;
u64 g_vk_occ_guards;
u64 g_vk_occ_copy_batches, g_vk_occ_copy_calls;
u64 g_vk_occ_checked_draws, g_vk_occ_hidden_draws;
static VkOccBudget budget;
static int sampling_active(void) { return vk_occ_budget_active(&budget, vk_frame_id() + 1); }

static OccPool* current_pool(void) {
    if (!sampling_active()) return NULL;
    u64 generation = vk_frame_id() + 1;
    OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
    if (p->generation == generation) return p;

    if (p->pool && p->sampled) {

        struct { u64 samples, available; } retired[OCC_BUDGET] = {0};
        VkResult r = vkGetQueryPoolResults(g_vk.dev, p->pool, 0, p->sampled,
            sizeof retired, retired, sizeof retired[0],
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        u64 checked = 0, hidden = 0;
        if (r == VK_SUCCESS)
            for (u32 i = 0; i < p->sampled; i++) {
                if (!retired[i].available) continue;
                checked += p->guarded[i] + p->inverted[i];
                hidden += retired[i].samples ? p->inverted[i] : p->guarded[i];
            }
        g_vk_occ_checked_draws += checked;
        g_vk_occ_hidden_draws += hidden;
        vk_occ_budget_record(&budget, generation, p->generation, checked ? checked : p->sampled, hidden);
        p->sampled = 0;
        if (!sampling_active()) return NULL;
    }
    vk_render_end();
    VkCommandBuffer cb = vk_cmd();
    if (!p->pool) {
        VkQueryPoolCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                    .queryType = VK_QUERY_TYPE_OCCLUSION, .queryCount = OCC_BUDGET};
        if (!VK_OK(vkCreateQueryPool(g_vk.dev, &ci, NULL, &p->pool))) return NULL;
    }
    vkCmdResetQueryPool(cb, p->pool, 0, OCC_BUDGET);
    memset(p->owner, 0, sizeof p->owner);
    memset(p->copied, 0, sizeof p->copied);
    memset(p->finished, 0, sizeof p->finished);
    p->finished_count = p->flushed = 0;
    memset(p->guarded, 0, sizeof p->guarded);
    memset(p->inverted, 0, sizeof p->inverted);
    p->used = 0;
    p->sampled = 0;
    p->generation = generation;
    return p;
}

void vk_occlusion_reset(u32 channel) {
    if (!sampling_active() || channel >= 128 || g_gpu_force_visible != 2 ||
        !g_vk.has_conditional_rendering || !g_vk.feat.occlusionQueryPrecise) return;
    OccScope* s = &scopes[channel];
    u64 epoch = s->epoch + 1;
    memset(s, 0, sizeof *s);
    s->epoch = epoch;
    s->generation = vk_frame_id() + 1;
    s->active = 1;
    if (!current_pool()) s->uncertain = 1;
}

VkOccTicket vk_occlusion_prepare(u32 channel, int safe32, int proxy) {
    VkOccTicket ticket = {0};
    if (!sampling_active() || channel >= 128 || g_gpu_force_visible != 2 ||
        !g_vk.has_conditional_rendering || !g_vk.feat.occlusionQueryPrecise) return ticket;
    OccScope* s = &scopes[channel];
    if (!s->active || s->uncertain) return ticket;
    OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
    if (s->generation != vk_frame_id() + 1 || p->generation != s->generation ||
        p->used == OCC_QUERIES || s->count == OCC_SCOPE) {
        s->uncertain = 1;
        return ticket;
    }
    if (!proxy || !safe32 || p->sampled >= OCC_BUDGET) {

        p->used++;
        s->uncertain = 1;
        return ticket;
    }
    ticket.pool = p->pool;
    ticket.query = p->used++;
    ticket.gpu_query = p->sampled++;
    p->gpu_indices[ticket.query] = ticket.gpu_query;
    ticket.channel = channel;
    ticket.generation = s->generation;
    ticket.safe32 = safe32;
    return ticket;
}

void vk_occlusion_begin(VkCommandBuffer cb, VkOccTicket* ticket) {
    if (!ticket->pool) return;
    OccScope* s = &scopes[ticket->channel];
    if (ticket->generation != vk_frame_id() + 1 || s->generation != ticket->generation) {
        s->uncertain = 1;
        ticket->pool = VK_NULL_HANDLE;
        return;
    }
    s->indices[s->count++] = ticket->query;
    s->gate = vk_submit_number();
    OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
    p->owner[ticket->query] = ticket->channel + 1;
    p->safe32[ticket->query] = ticket->safe32;

    vkCmdBeginQuery(cb, ticket->pool, ticket->gpu_query,
        g_vk.feat.occlusionQueryPrecise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
    g_vk_occ_queries++;
}
void vk_occlusion_end(VkCommandBuffer cb, const VkOccTicket* ticket) {
    if (ticket->pool) {
        vkCmdEndQuery(cb, ticket->pool, ticket->gpu_query);
        OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
        if (p->generation == ticket->generation && !p->finished[ticket->query]) {
            p->finished[ticket->query] = 1;
            p->finished_indices[p->finished_count++] = ticket->query;
        }
    }
}

static OccMarker* marker(u64 address) {
    u64 h = address * UINT64_C(0x9e3779b97f4a7c15);
    return &markers[(h ^ (h >> 32)) & 4095];
}
void vk_occlusion_invalidate(u64 address, u32 size) {
    if (!size || size > 16) return;
    u64 first = address & ~UINT64_C(15);
    u64 last = (address + size - 1) & ~UINT64_C(15);
    OccMarker* a = marker(first);
    if (a->address == first) a->generation = 0;
    OccMarker* b = marker(last);
    if (b->address == last) b->generation = 0;
}
void vk_occlusion_mark(u32 channel, u64 address) {
    if (!sampling_active() || channel >= 128 || !address || (address & 15) || g_gpu_force_visible != 2 ||
        !g_vk.has_conditional_rendering || !g_vk.feat.occlusionQueryPrecise) return;
    OccPool* p = current_pool();
    if (!p) return;
    OccScope* s = &scopes[channel];
    *marker(address) = (OccMarker){address, p->generation, s->epoch, channel, p->used};
    s->active = 1;
    s->uncertain = 0;
    s->count = 0;
    s->generation = p->generation;
}


int vk_occlusion_condition(const GpuChannel* ch, VkConditionalRenderingBeginInfoEXT* condition) {
    const u32* r = ch->r3d;
    u32 mode = r[0x556] & 7;
    if (!sampling_active() || g_gpu_force_visible != 2 || !g_vk.has_conditional_rendering ||
        !g_vk.feat.occlusionQueryPrecise || (r[0x651] & 3) || (mode != 3 && mode != 4) || !r[0x4B3]) return 0;
    u64 address = ((u64)(r[0x554] & 255) << 32) | r[0x555];
    const OccMarker *a = marker(address), *b = marker(address + 16);
    OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
    if (!address || a->address != address || b->address != address + 16 ||
        a->generation != vk_frame_id() + 1 || b->generation != a->generation ||
        a->channel != ch->id || b->channel != ch->id || a->epoch != b->epoch) goto unknown;
    u32 lo = a->cursor < b->cursor ? a->cursor : b->cursor;
    u32 hi = a->cursor > b->cursor ? a->cursor : b->cursor;
    if (hi - lo != 1 || hi > p->used || p->owner[lo] != ch->id + 1 || !p->safe32[lo] || !p->finished[lo]) goto unknown;
    if (!p->predicates) {
        VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = OCC_QUERIES * sizeof(u32),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                     VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        if (!VK_OK(vmaCreateBuffer(g_vk.vma, &bi, &ai, &p->predicates, &p->memory, NULL))) goto unknown;
    }
    if (!p->copied[lo]) {
        vk_render_end();
        VkCommandBuffer cb = vk_cmd();

        while (p->flushed < p->finished_count) {
            u32 first = p->finished_indices[p->flushed++], count = 1;
            while (p->flushed < p->finished_count) {
                u32 next = p->finished_indices[p->flushed];
                if (next != first + count || p->gpu_indices[next] != p->gpu_indices[first] + count) break;
                count++; p->flushed++;
            }
            vkCmdCopyQueryPoolResults(cb, p->pool, p->gpu_indices[first], count,
                p->predicates, (u64)first * 4, 4, VK_QUERY_RESULT_WAIT_BIT);
            for (u32 i = 0; i < count; i++) p->copied[first + i] = 1;
            g_vk_occ_copy_calls++;
        }
        g_vk_occ_copy_batches++;
        VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT, .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,
            .dstAccessMask = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT};
        VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(cb, &di);
        p->copied[lo] = 1;
    }
    *condition = (VkConditionalRenderingBeginInfoEXT){.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        .buffer = p->predicates, .offset = (u64)lo * 4,
        .flags = mode == 3 ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0};
    g_vk_occ_guards++;
    {
        u32 physical = p->gpu_indices[lo];
        if (mode == 3) p->inverted[physical]++;
        else p->guarded[physical]++;
    }
    return 1;
unknown:
    g_vk_occ_fallback++;
    return 0;
}

int vk_occlusion_report(u32 channel, u64* samples) {
    if (channel >= 128) return 0;
    OccScope* s = &scopes[channel];
    OccPool* p = &pools[vk_frame_id() % VK_FRAMES];
    int usable = s->active && !s->uncertain && s->count &&
                 s->generation == vk_frame_id() + 1 && p->generation == s->generation;

    s->active = 0;
    if (!usable || s->gate > vk_completed_submit()) goto unknown;
    u32 first = p->gpu_indices[s->indices[0]], count = p->gpu_indices[s->indices[s->count - 1]] - first + 1;
    memset(results, 0, (size_t)count * sizeof results[0]);
    VkResult r = vkGetQueryPoolResults(g_vk.dev, p->pool, first, count,
        (size_t)count * sizeof results[0], results, sizeof results[0],
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (r != VK_SUCCESS) goto unknown;
    u64 total = 0;
    for (u32 i = 0; i < s->count; i++) {
        u32 at = p->gpu_indices[s->indices[i]] - first;
        if (!results[at].available) goto unknown;
        u64 n = results[at].samples;
        total = n > UINT64_MAX - total ? UINT64_MAX : total + n;
    }
    *samples = total;
    g_vk_occ_ready++;
    return 1;
unknown:
    g_vk_occ_fallback++;
    return 0;
}
