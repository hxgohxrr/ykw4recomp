/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include <stdlib.h>
#include <string.h>
#include "nx/gpu/gpu.h"
#include "nx/gpu/renderer.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/memory/mem.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include "nx/gpu/vk/vk.h"

typedef struct Vb {
    u64 start, len, hash, shash;
    u32 epoch, strict_epoch, chk_epoch, schk, stable, used;
    u64 gw;     
    u64 frame;  
    int own;    
    u32 wseen;  
    VkBuffer buf;
    VmaAllocation mem;
    u64 off;
    struct Vb* next;
} Vb;

#define VBH 65536
static Vb* g_vbh[VBH];

static u64 g_copy_stage, g_copy_owned, g_copy_uncached, g_copy_zero, g_copy_calls, g_copy_peak, g_read_bound;
static u64 g_wb_outputs, g_wb_bytes, g_wb_waits, g_wb_wait_ns, g_wb_reuses;
static u64 g_alias_draws, g_alias_pages;
enum { WB_DRAW, WB_READ, WB_WRITE, WB_SIGNAL, WB_SYNCWAIT, WB_MAP, WB_REASON_N };
static u64 g_wb_reason[WB_REASON_N], g_wb_reason_ns[WB_REASON_N], g_wb_draw_miss[4];
static int g_copy_stats = -1;
static int copy_stats_on(void) {
    if (g_copy_stats < 0) g_copy_stats = getenv("SNX_VK_BUFFER_STATS") != NULL;
    return g_copy_stats;
}
static void copy_note(u64 gva, u64 len) {
    if (!copy_stats_on()) return;
    g_copy_uncached += len;
    g_copy_calls++;
    if (len > g_copy_peak) g_copy_peak = len;
    if (!gva) g_copy_zero += len;
}

typedef struct GfxOut {
    u64 addr, frame, frame_id;  
    u32 size;
    VkBuffer buf;
    u64 off;
    VmaAllocation mem;
    u8* ptr;
    u8* org;
    u64* pages;  
    u32 npages;
    struct GfxOut* next;
} GfxOut;
static GfxOut *g_prep, *g_prep_tail, *g_out, *g_out_tail;

static GfxOut* gfx_out_new(u32 npages, u32 len) {
    GfxOut* o = calloc(1, sizeof *o + (size_t)npages * sizeof *o->pages + len);
    if (!o) return NULL;
    o->pages = (u64*)(o + 1);
    o->npages = npages;
    o->org = (u8*)(o->pages + npages);
    return o;
}


static _Thread_local int g_out_landing;

void vk_buf_writable_abort(void) {
    while (g_prep) {
        GfxOut* o = g_prep;
        g_prep = o->next;
        free(o);
    }
    g_prep_tail = NULL;
}
void vk_buf_writable_commit(void) {
    if (!g_prep) return;
    if (g_out_tail)
        g_out_tail->next = g_prep;
    else
        g_out = g_prep;
    g_out_tail = g_prep_tail;
    g_prep = g_prep_tail = NULL;
    g_gfx_any = 1;
}

void vk_buf_frame_done(u64 frame) {
    GfxOut *done = NULL, *done_tail = NULL;
    for (GfxOut** pp = &g_out; *pp;) {
        GfxOut* o = *pp;
        if (o->frame != frame) {
            pp = &o->next;
            continue;
        }
        *pp = o->next;
        o->next = NULL;
        if (done_tail)
            done_tail->next = o;
        else
            done = o;
        done_tail = o;
    }
    g_out_tail = NULL;
    for (GfxOut* o = g_out; o; o = o->next) g_out_tail = o;
    g_gfx_any = g_out != NULL || done != NULL;
    VmaAllocation invalidated = VK_NULL_HANDLE;
    g_out_landing++;
    while (done) {
        GfxOut* o = done;
        done = o->next;
        if (o->mem != invalidated) {
            if (!VK_OK(vmaInvalidateAllocation(g_vk.vma, o->mem, 0, VK_WHOLE_SIZE)))
                fatal("Could not invalidate a graphics storage buffer after its frame completed");
            invalidated = o->mem;
        }
        vk_write_changed(o->addr, o->ptr, o->org, o->size);
        if (copy_stats_on()) {
            g_wb_outputs++;
            g_wb_bytes += o->size;
        }
        free(o);
    }
    g_out_landing--;
    g_gfx_any = g_out != NULL;
}

void vk_buf_host_barrier(VkCommandBuffer cb, u64 frame) {
    GfxOut* first = g_out;
    while (first && first->frame != frame) first = first->next;
    if (!first) return;
    VkMemoryBarrier2 b = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                          .srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                          .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                          .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                          .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT};
    VkDependencyInfo d = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &b};
    vkCmdPipelineBarrier2(cb, &d);

    VkBufferCopy regions[64];
    u32 n = 0;
    VkBuffer src = VK_NULL_HANDLE, dst = VK_NULL_HANDLE;
    for (GfxOut* o = first; o; o = o->next) {
        if (o->frame != frame) continue;
        VkBuffer back;
        u64 at;
        VmaAllocation mem;
        u8* p = vk_stage_back(o->size, &back, &at, &mem);
        if (!p) break;  
        if (n && (n == sizeof regions / sizeof *regions || o->buf != src || back != dst)) {
            vkCmdCopyBuffer(cb, src, dst, n, regions);
            n = 0;
        }
        src = o->buf;
        dst = back;
        regions[n++] = (VkBufferCopy){.srcOffset = o->off, .dstOffset = at, .size = o->size};
        o->ptr = p;
        o->mem = mem;
    }
    if (n) vkCmdCopyBuffer(cb, src, dst, n, regions);
    b.srcStageMask |= VK_PIPELINE_STAGE_2_COPY_BIT;
    b.srcAccessMask |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    b.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(cb, &d);
}
static void wait_finish(int reason) {
    u64 t = copy_stats_on() ? plat_ns() : 0;
    vk_finish();
    if (t) {
        u64 ns = plat_ns() - t;
        g_wb_waits++;
        g_wb_wait_ns += ns;
        g_wb_reason[reason]++;
        g_wb_reason_ns[reason] += ns;
    }
}
static void land_reason(int reason) {
    if (!g_gfx_any || g_out_landing) return;
    vk_lock();
    if (g_gfx_any && !g_out_landing) wait_finish(reason);
    vk_unlock();
}
void vk_buf_land(void) { land_reason(WB_SIGNAL); }
void vk_buf_land_syncwait(void) { land_reason(WB_SYNCWAIT); }
void vk_buf_land_map(void) { land_reason(WB_MAP); }

static int page_cmp(const void* a, const void* b) {
    u64 x = *(const u64*)a, y = *(const u64*)b;
    return (x > y) - (x < y);
}

typedef struct {
    u64 page[16];
    u32 n;
    u64 lo, hi;  
    u64 a, an;
} RangePages;
static void range_pages(u64 a, u64 an, RangePages* q) {
    q->n = 0;
    q->lo = ~0ull;
    q->hi = 0;
    q->a = a;
    q->an = an;
    for (u64 ap = a >> 12, ae = (a + an - 1) >> 12; ap <= ae; ap++) {
        u64 ac = gmmu_translate(ap << 12), page;
        if (!ac) continue;
        page = ac >> 12;
        if (q->n < 16) q->page[q->n] = page;
        q->n++;
        if (page < q->lo) q->lo = page;
        if (page > q->hi) q->hi = page;
    }
}
static int backing_overlap(const RangePages* q, const GfxOut* o) {
    u64 olo, ohi;
    u32 i;
    if (!q->n || !o->npages) return 0;
    olo = o->pages[0];
    ohi = o->pages[o->npages - 1];
    if (ohi == ~0ull)  
        for (i = o->npages; i-- && o->pages[i] == ~0ull;) ohi = i ? o->pages[i - 1] : 0;
    if (q->hi < olo || ohi < q->lo) return 0;
    if (q->n <= 16) {
        for (i = 0; i < q->n; i++)
            if (bsearch(&q->page[i], o->pages, o->npages, sizeof q->page[i], page_cmp)) return 1;
        return 0;
    }
    for (u64 ap = q->a >> 12, ae = (q->a + q->an - 1) >> 12; ap <= ae; ap++) {
        u64 ac = gmmu_translate(ap << 12);
        if (!ac) continue;
        u64 page = ac >> 12;
        if (bsearch(&page, o->pages, o->npages, sizeof page, page_cmp)) return 1;
    }
    return 0;
}
static void mem_need_reason(u64 gva, u64 len, int reason) {
    RangePages q;
    if (!g_gfx_any || g_out_landing || !len) return;
    vk_lock();
    range_pages(gva, len, &q);

    u64 newest = 0;
    int recording = 0;
    for (GfxOut* o = g_out; o; o = o->next)
        if ((gva < o->addr + o->size && o->addr < gva + len) || backing_overlap(&q, o)) {
            if (o->frame > vk_last_submit()) { recording = 1; break; }  
            if (o->frame > newest) newest = o->frame;
        }
    if (recording) wait_finish(reason);
    else if (newest) {
        u64 t = copy_stats_on() ? plat_ns() : 0;
        vk_wait_submit(newest);
        vk_land_through(newest);
        if (t) {
            u64 ns = plat_ns() - t;
            g_wb_waits++; g_wb_wait_ns += ns;
            g_wb_reason[reason]++; g_wb_reason_ns[reason] += ns;
        }
    }
    vk_unlock();
}
void vk_buf_mem_need(u64 gva, u64 len) { mem_need_reason(gva, len, WB_READ); }
int vk_buf_pending_any(void) { return g_gfx_any; }
int vk_buf_pending_unsubmitted(void) {
    for (GfxOut* o = g_out; o; o = o->next)
        if (o->frame == vk_submit_number()) return 1;
    return 0;
}
int vk_buf_pending(u64 gva, u64 len) {
    RangePages q;
    if (!g_gfx_any || !len) return 0;
    range_pages(gva, len, &q);
    for (GfxOut* o = g_out; o; o = o->next)
        if ((gva < o->addr + o->size && o->addr < gva + len) || backing_overlap(&q, o)) return 1;
    return 0;
}
void vk_buf_mem_need_map(u64 gva, u64 len) { mem_need_reason(gva, len, WB_MAP); }
void vk_buf_before_write(u64 gva, u64 len) {
    if (len)
        mem_need_reason(gva, len, WB_WRITE);
    else
        land_reason(WB_SIGNAL);
}

typedef struct {
    VkBufferCopy regions[64];
    u64 written[128][2];  
    u32 n, nwritten;
    VkBuffer src, dst;
    int started;
} CopyBatch;
static void copy_barrier(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                         VkAccessFlags2 dst_access) {
    VkMemoryBarrier2 b = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                          .srcStageMask = src_stage,
                          .srcAccessMask = src_access,
                          .dstStageMask = dst_stage,
                          .dstAccessMask = dst_access};
    VkDependencyInfo d = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &b};
    vkCmdPipelineBarrier2(vk_cmd(), &d);
}
static void copy_flush(CopyBatch* c) {
    if (c->n) vkCmdCopyBuffer(vk_cmd(), c->src, c->dst, c->n, c->regions);
    c->n = 0;
}
static void copy_add(CopyBatch* c, VkBuffer src, u64 src_off, u64 dst_off, u64 size) {
    u32 k;
    int overlap = c->nwritten == sizeof c->written / sizeof *c->written;
    if (!c->started) {
        vk_render_end();
        copy_barrier(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
        c->started = 1;
    }
    for (k = 0; k < c->nwritten && !overlap; k++)
        overlap = dst_off < c->written[k][1] && c->written[k][0] < dst_off + size;
    if (overlap) {
        copy_flush(c);
        copy_barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT);
        c->nwritten = 0;
    }
    if (c->n && (c->n == sizeof c->regions / sizeof *c->regions || src != c->src)) copy_flush(c);
    c->src = src;
    c->regions[c->n++] = (VkBufferCopy){.srcOffset = src_off, .dstOffset = dst_off, .size = size};
    c->written[c->nwritten][0] = dst_off;
    c->written[c->nwritten][1] = dst_off + size;
    c->nwritten++;
}
static void copy_end(CopyBatch* c) {
    copy_flush(c);
    if (c->started)  
        copy_barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
}


static void read_untracked(u64 gva, u8* dst, u64 len) {
    while (len) {
        u64 n = 4096 - (gva & 4095);
        const u8* p;
        if (n > len) n = len;
        p = gmem_ptr_untracked(gva, n);
        if (p)
            memcpy(dst, p, n);
        else
            memset(dst, 0, n);
        gva += n;
        dst += n;
        len -= n;
    }
}

static int out_copyable(const GfxOut* o, u64 lo, u64 hi) {
    const u8* host = gmem_ptr_untracked(lo, hi - lo);
    return o->frame_id == vk_frame_id() && host && !memcmp(host, o->org + (lo - o->addr), hi - lo);
}

static void gfx_overlay(u64 gva, u64 len, VkBuffer dst, u64 dst_off) {
    CopyBatch c;
    memset(&c, 0, sizeof c);
    c.dst = dst;
    for (GfxOut* o = g_out; o; o = o->next) {
        u64 lo = gva > o->addr ? gva : o->addr, hi = gva + len < o->addr + o->size ? gva + len : o->addr + o->size;
        if (lo < hi) copy_add(&c, o->buf, o->off + (lo - o->addr), dst_off + (lo - gva), hi - lo);
    }
    copy_end(&c);
}


static int gfx_gpu_src(u64 gva, u64 len, VkBuffer* buf, u64* off, int writable) {
    GfxOut* match = NULL;
    int count = 0;
    RangePages q;
    if (!g_gfx_any || g_out_landing || !gva || !len) return 0;
    if (g_cs_any) vk_compute_mem_need(gva, len);
    range_pages(gva, len, &q);
    for (GfxOut* o = g_out; o; o = o->next) {
        u64 lo = gva > o->addr ? gva : o->addr, hi = gva + len < o->addr + o->size ? gva + len : o->addr + o->size;
        if (lo >= hi) {
            if (!backing_overlap(&q, o)) continue;
            land_reason(WB_DRAW);
            return 0;
        }
        if (!out_copyable(o, lo, hi)) {
            if (copy_stats_on()) g_wb_draw_miss[o->frame_id != vk_frame_id() ? 0 : 2]++;
            land_reason(WB_DRAW);
            return 0;
        }
        match = o;
        count++;
    }
    if (!count) return 0;
    if (count == 1 && gva >= match->addr && gva + len <= match->addr + match->size &&
        !((match->off + (gva - match->addr)) & (vk_stage_align() - 1)) &&
        (!writable || match->frame == vk_submit_number())) {

        static u64 barrier_frame = ~0ull, barrier_work = ~0ull;
        if (barrier_frame != vk_submit_number() || barrier_work != g_vk_drawn + g_vk_dispatches) {
            vk_render_end();
            copy_barrier(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
            barrier_frame = vk_submit_number();
            barrier_work = g_vk_drawn + g_vk_dispatches;
        }
        *buf = match->buf;
        *off = match->off + (gva - match->addr);
        if (copy_stats_on()) g_wb_reuses++;
        return 1;
    }
    if (copy_stats_on()) g_wb_draw_miss[count > 1 ? 1 : 3]++;  
    return 2;
}

static int gfx_composed(u64 gva, u64 len, VkBuffer* buf, u64* off) {
    u8* p = vk_stage(len, vk_stage_align(), buf, off);
    if (!p) return 0;
    read_untracked(gva, p, len);
    gfx_overlay(gva, len, *buf, *off);
    return 1;
}


#define WW_LEAF_SHIFT 22
static u32* g_wleaf[AS_SIZE >> WW_LEAF_SHIFT];
static u32 g_wleaf_max[AS_SIZE >> WW_LEAF_SHIFT];
static u32 g_wclock = 1;
static int g_ww = -1;  
static void ww_mark(u8* p, u64 n) {
    u64 o = (u64)(p - g_mem);
    if (o >= AS_SIZE || !n) return;
    for (u64 pg = o >> 12, e = (o + n - 1) >> 12; pg <= e && pg < (AS_SIZE >> 12); pg++) {
        u64 l = pg >> (WW_LEAF_SHIFT - 12);
        if (!g_wleaf[l]) {

            u32* t = calloc(1u << (WW_LEAF_SHIFT - 12), 4);
            if (t && g_wleaf_max[l])
                for (u32 i = 0; i < 1u << (WW_LEAF_SHIFT - 12); i++) t[i] = g_wleaf_max[l];
            g_wleaf[l] = t;
        }
        if (g_wleaf[l]) g_wleaf[l][pg & ((1u << (WW_LEAF_SHIFT - 12)) - 1)] = g_wclock;
        g_wleaf_max[l] = g_wclock;
    }
}

static void ww_mark_range(u64 o, u64 e) {
    const u64 lp = 1ull << (WW_LEAF_SHIFT - 12);
    for (u64 pg = o >> 12, pe = (e + 4095) >> 12; pg < pe;) {
        u64 l = pg >> (WW_LEAF_SHIFT - 12), lend = (l + 1) * lp, end = pe < lend ? pe : lend;
        if (!g_wleaf[l] && pg == l * lp && end == lend)
            g_wleaf_max[l] = g_wclock;
        else
            ww_mark(g_mem + (pg << 12), (end - pg) << 12);
        pg = end;
    }
}


typedef struct {
    u64 a, e;
} WwRange;
#define WW_CHG_N 32
static WwRange g_wchg[WW_CHG_N];
static int g_wchg_n;  
static PMutex g_wchg_lock = PMUTEX_INIT;
void vk_mem_changed(u64 cpu, u64 len) {
    if (cpu >= AS_SIZE || !len) return;
    u64 e = len > AS_SIZE - cpu ? AS_SIZE : cpu + len;
    pm_lock(&g_wchg_lock);
    int n = __atomic_load_n(&g_wchg_n, __ATOMIC_RELAXED);
    if (n < WW_CHG_N) {
        g_wchg[n] = (WwRange){cpu, e};
        __atomic_store_n(&g_wchg_n, n + 1, __ATOMIC_RELEASE);
    } else {
        WwRange* r = &g_wchg[WW_CHG_N - 1];
        if (cpu < r->a) r->a = cpu;
        if (e > r->e) r->e = e;
    }
    pm_unlock(&g_wchg_lock);
}
static void ww_take_changed(void) {
    if (!__atomic_load_n(&g_wchg_n, __ATOMIC_ACQUIRE)) return;
    WwRange c[WW_CHG_N];
    pm_lock(&g_wchg_lock);
    int n = __atomic_load_n(&g_wchg_n, __ATOMIC_RELAXED);
    memcpy(c, g_wchg, (size_t)n * sizeof *c);
    __atomic_store_n(&g_wchg_n, 0, __ATOMIC_RELAXED);
    pm_unlock(&g_wchg_lock);
    for (int i = 0; i < n; i++) ww_mark_range(c[i].a, c[i].e);
}

int vk_written_since(const u8* host, u64 len, u32 since) {
    if (!g_ww || !host || !len) return -1;
    if (g_ww < 0) {
        g_ww = plat_ww_scan(g_mem, 4096, ww_mark) == 0;
        LOG("vulkan: %s\n",
            g_ww ? "write tracking on (caches skip unchanged memory)" : "no write tracking (caches check contents)");
        if (!g_ww) return -1;
    }

    if (g_wclock == UINT32_MAX) {
        g_ww = 0;
        return -1;
    }
    g_wclock++;
    ww_take_changed();
    if (plat_ww_scan((u8*)host, len, ww_mark) < 0) {
        g_ww = 0;
        LOG("vulkan: write tracking failed, caches check contents\n");
        return -1;
    }
    u64 o = (u64)(host - g_mem);
    for (u64 pg = o >> 12, e = (o + len - 1) >> 12; pg <= e;) {
        u64 l = pg >> (WW_LEAF_SHIFT - 12), lend = (l + 1) << (WW_LEAF_SHIFT - 12);
        if (g_wleaf_max[l] > since) {
            if (!g_wleaf[l]) return 1;  
            for (u64 q = pg; q <= e && q < lend; q++)
                if (g_wleaf[l][q & ((1u << (WW_LEAF_SHIFT - 12)) - 1)] > since) return 1;
        }
        pg = lend;
    }
    return 0;
}
u32 vk_ww_now(void) { return g_wclock; }

static u8* g_tmp;
static u64 g_tmp_cap;

static const u8* guest_bytes(u64 gva, u64 len) {
    u8* p = gmem_ptr(gva, len);
    if (p) return p;
    if (g_tmp_cap < len) {
        u8* n = realloc(g_tmp, len);
        if (!n) return NULL;
        g_tmp = n;
        g_tmp_cap = len;
    }
    gmem_read(gva, g_tmp, len);
    return g_tmp;
}

int vk_buf_copy(u64 gva, u64 len, VkBuffer* buf, u64* off) {
    int src = gva ? gfx_gpu_src(gva, len, buf, off, 0) : 0;
    if (src == 1) return 1;
    if (src == 2) return gfx_composed(gva, len, buf, off);
    u8* p = vk_stage(len ? len : 16, vk_stage_align(), buf, off);
    if (!p) return 0;
    if (!gva)
        memset(p, 0, len);
    else {
        const u8* src = guest_bytes(gva, len);
        if (!src) return 0;
        memcpy(p, src, len);
    }
    copy_note(gva, len);
    return 1;
}


int vk_buf_copy_writable(u64 gva, u32 len, VkBuffer* buf, u64* off) {
    GfxOut* o;
    if (!gva) return vk_buf_copy(0, len, buf, off);

    for (GfxOut* p = g_prep; p; p = p->next)
        if (p->addr == gva && p->size == len) {
            *buf = p->buf;
            *off = p->off;
            return 1;
        }
    int pending = gfx_gpu_src(gva, len, buf, off, 1);
    if (pending == 1) return 1;
    if (pending == 2) {

        land_reason(WB_DRAW);
    }
    o = gfx_out_new((u32)(((gva & 0xFFF) + len + 0xFFF) >> 12), len);
    if (!o) return 0;
    const u8* src = guest_bytes(gva, len);
    if (!src) {
        free(o);
        return 0;
    }
    memcpy(o->org, src, len);
    for (u32 i = 0; i < o->npages; i++) {
        u64 cpu = gmmu_translate((gva & ~0xFFFull) + (u64)i * 0x1000);
        o->pages[i] = cpu ? cpu >> 12 : ~0ull;
    }
    qsort(o->pages, o->npages, sizeof *o->pages, page_cmp);
    u64 align = vk_stage_align();
    if (align < g_vk.props.limits.nonCoherentAtomSize) align = g_vk.props.limits.nonCoherentAtomSize;
    o->ptr = vk_stage_output(len, align, buf, off, &o->mem);
    if (!o->ptr) {
        free(o);
        return 0;
    }
    memcpy(o->ptr, o->org, len);
    if (pending == 2) gfx_overlay(gva, len, *buf, *off);
    o->addr = gva;
    o->size = len;
    o->buf = *buf;
    o->off = *off;
    o->frame = vk_submit_number();
    o->frame_id = vk_frame_id();
    if (g_prep_tail)
        g_prep_tail->next = o;
    else
        g_prep = o;
    g_prep_tail = o;
    copy_note(gva, len);
    return 1;
}

typedef struct {
    u64 key, gva;
    int writable, bind;
} DrawPage;
static int draw_page_cmp(const void* a, const void* b) {
    u64 x = ((const DrawPage*)a)->key, y = ((const DrawPage*)b)->key;
    return (x > y) - (x < y);
}
static u64 draw_page_key(u64 gva) {
    u64 cpu = gmmu_translate(gva);

    return cpu ? cpu >> 12 : (1ull << 63) | (gva >> 12);
}
static DrawPage* draw_pages(const VkDrawStorage* draw, size_t* count) {
    size_t n = 0;
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < draw->n[s]; i++) {
            const VkDrawGbuf* b = &draw->b[s][i];
            if (b->addr) n += ((b->addr & 4095) + b->size + 4095) >> 12;
        }
    DrawPage* p = malloc((n ? n : 1) * sizeof *p);
    if (!p) return NULL;
    size_t k = 0;
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < draw->n[s]; i++) {
            const VkDrawGbuf* b = &draw->b[s][i];
            if (!b->addr) continue;
            for (u64 g = b->addr & ~4095ull, e = b->addr + b->size; g < e; g += 4096)
                p[k++] = (DrawPage){draw_page_key(g), g, b->writable, s * 16 + i};
        }
    qsort(p, k, sizeof *p, draw_page_cmp);
    *count = k;
    return p;
}
static int draw_alias_bindings(const DrawPage* p, size_t n, u8 active[32]) {
    memset(active, 0, 32);
    int alias = 0;
    for (size_t i = 0; i < n;) {
        size_t j = i + 1;
        int writable = p[i].writable;
        while (j < n && p[j].key == p[i].key) writable |= p[j++].writable;
        if (j - i > 1 && writable) {
            for (size_t k = i; k < j; k++) active[p[k].bind] = 1;
            alias = 1;
        }
        i = j;
    }
    return alias;
}
static int draw_has_writable(const VkDrawStorage* draw) {
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < draw->n[s]; i++)
            if (draw->b[s][i].addr && draw->b[s][i].writable) return 1;
    return 0;
}
int vk_buf_draw_has_alias(const VkDrawStorage* draw) {
    if (!draw_has_writable(draw)) return 0;
    size_t n;
    DrawPage* p = draw_pages(draw, &n);
    if (!p) return -1;
    u8 active[32];
    int alias = draw_alias_bindings(p, n, active);
    free(p);
    return alias;
}
static u32 draw_page_index(const DrawPage* p, size_t n, u64 key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (p[mid].key < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    return (u32)lo;
}
static int draw_output_page(const DrawPage* p, u8* ptr, VkBuffer buf, u64 off, VmaAllocation mem) {
    GfxOut* o;
    const u8* page;
    if (p->key & (1ull << 63)) return 1;
    o = gfx_out_new(1, 4096);
    if (!o) return 0;

    page = gmem_ptr_untracked(p->gva, 4096);
    if (page)
        memcpy(o->org, page, 4096);
    else
        memset(o->org, 0, 4096);
    o->pages[0] = p->key;
    o->addr = p->gva;
    o->size = 4096;
    o->frame = vk_submit_number();
    o->frame_id = vk_frame_id();
    o->buf = buf;
    o->off = off;
    o->mem = mem;
    o->ptr = ptr;
    if (g_prep_tail)
        g_prep_tail->next = o;
    else
        g_prep = o;
    g_prep_tail = o;
    return 1;
}

static void atlas_fill(const DrawPage* p, size_t unique, u8* ptr, VkBuffer atlas, u64 atlas_off) {
    CopyBatch c;
    size_t i;
    if (g_cs_any)
        for (i = 0; i < unique; i++)
            if (!(p[i].key & (1ull << 63))) vk_compute_mem_need(p[i].gva, 4096);

    for (i = 0; i < unique; i++)
        if (!(p[i].key & (1ull << 63))) mem_need_reason(p[i].gva, 4096, WB_DRAW);
    for (i = 0; i < unique; i++) {
        const u8* page = p[i].key & (1ull << 63) ? NULL : gmem_ptr_untracked(p[i].gva, 4096);
        if (page)
            memcpy(ptr + i * 4096, page, 4096);
        else
            memset(ptr + i * 4096, 0, 4096);
    }
    memset(&c, 0, sizeof c);
    c.dst = atlas;
    for (GfxOut* o = g_out; o && !g_out_landing; o = o->next)
        for (u64 g = o->addr & ~4095ull, e = o->addr + o->size; g < e; g += 4096) {
            u64 key = draw_page_key(g), lo = g > o->addr ? g : o->addr, hi = g + 4096 < e ? g + 4096 : e;
            u32 at = draw_page_index(p, unique, key);
            if (at < unique && p[at].key == key)
                copy_add(&c, o->buf, o->off + (lo - o->addr), atlas_off + at * 4096ull + (lo - g), hi - lo);
        }
    copy_end(&c);
}


int vk_buf_draw_prepare(VkDrawStorage* draw) {
    size_t n = 0;
    int writable = draw_has_writable(draw);
    DrawPage* p = writable ? draw_pages(draw, &n) : NULL;
    if (writable && !p) return 0;
    u8 active[32] = {0};
    int alias = p ? draw_alias_bindings(p, n, active) : 0;
    size_t unique = 0;
    if (alias)
        for (size_t i = 0; i < n; i++)
            if (active[p[i].bind] && (!unique || p[i].key != p[unique - 1].key)) p[unique++] = p[i];
    VkBuffer atlas = VK_NULL_HANDLE;
    u64 atlas_off = 0, atlas_size = unique * 4096ull;
    VmaAllocation mem = VK_NULL_HANDLE;
    u8* ptr = NULL;
    if (alias) {
        if (atlas_size > g_vk.props.limits.maxStorageBufferRange) {
            WARN_LIMIT(1, "vulkan: aliased draw needs %llu storage bytes, beyond the device's descriptor range\n",
                       (unsigned long long)atlas_size);
            free(p);
            return 0;
        }
        u64 align = vk_stage_align();
        if (align < g_vk.props.limits.nonCoherentAtomSize) align = g_vk.props.limits.nonCoherentAtomSize;
        ptr = vk_stage_output(atlas_size, align, &atlas, &atlas_off, &mem);
        if (!ptr) {
            free(p);
            return 0;
        }
        atlas_fill(p, unique, ptr, atlas, atlas_off);
        copy_note(1, atlas_size);
    }
    for (int s = 0; s < 2; s++) {
        if (!draw->n[s]) continue;
        size_t words = 48;
        if (alias)
            for (int i = 0; i < draw->n[s]; i++) {
                VkDrawGbuf* b = &draw->b[s][i];
                if (b->addr && active[s * 16 + i]) words += ((b->addr & 4095) + b->size + 4095) >> 12;
            }
        u32* map = (u32*)vk_stage(words * 4, vk_stage_align(), &draw->map_buf[s], &draw->map_off[s]);
        if (!map) {
            free(p);
            return 0;
        }
        draw->map_size[s] = words * 4;
        for (int i = 0; i < 16; i++) map[i] = ~0u;
        for (int i = 16; i < 32; i++) map[i] = 0;
        for (int i = 32; i < 48; i++) map[i] = 0;
        size_t at = 48;
        if (alias)
            for (int i = 0; i < draw->n[s]; i++) {
                VkDrawGbuf* b = &draw->b[s][i];
                if (!b->addr || !active[s * 16 + i]) continue;
                map[i] = (u32)at;
                map[16 + i] = (u32)((b->addr & 4095) >> 2);
                map[32 + i] = b->size >> 2;
                for (u64 g = b->addr & ~4095ull, e = b->addr + b->size; g < e; g += 4096)
                    map[at++] = draw_page_index(p, unique, draw_page_key(g));
                b->aliased = 1;
                b->buf = atlas;
                b->off = atlas_off;
                b->range = atlas_size;
            }
    }
    if (alias)
        for (size_t i = 0; i < unique; i++)
            if (!draw_output_page(&p[i], ptr + i * 4096, atlas, atlas_off + i * 4096, mem)) {
                vk_buf_writable_abort();
                free(p);
                return 0;
            }
    if (alias && copy_stats_on()) {
        g_alias_draws++;
        g_alias_pages += unique;
    }
    free(p);
    return 1;
}


static void use(Vb* e, VkBuffer* buf, u64* off) {
    e->used = g_vk_present;
    if (copy_stats_on()) g_read_bound += e->len;
    *buf = e->buf;
    *off = e->off;
}
static void drop_own(Vb* e) {
    if (e->own) vk_defer_buffer(e->buf, e->mem);
    e->own = 0;
    e->buf = VK_NULL_HANDLE;
}
static int make_own(Vb* e, const u8* src) {
    VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size = e->len,
                              .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                       VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo aci = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                            VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                   .usage = VMA_MEMORY_USAGE_AUTO};
    VmaAllocationInfo info;
    VkBuffer b;
    VmaAllocation m;
    if (vmaCreateBuffer(g_vk.vma, &bci, &aci, &b, &m, &info) != VK_SUCCESS) return 0;
    memcpy(info.pMappedData, src, e->len);
    if (copy_stats_on()) g_copy_owned += e->len;
    vmaFlushAllocation(g_vk.vma, m, 0, VK_WHOLE_SIZE);
    drop_own(e);
    e->own = 1;
    e->buf = b;
    e->mem = m;
    e->off = 0;
    return 1;
}

static int buf_get(u64 gva, u64 len, VkBuffer* buf, u64* off, int strict) {
    int pending;
    if (!len) return 0;
    pending = gfx_gpu_src(gva, len, buf, off, 0);
    if (pending == 1) return 1;
    if (pending == 2) return gfx_composed(gva, len, buf, off);  
    if (vk_compute_gpu_src(gva, len, buf, off)) return 1;       

    if (g_cs_any) vk_compute_mem_need(gva, len);

    u64 key = gva ^ (len + 0x9e3779b97f4a7c15ull + (gva << 6) + (gva >> 2));
    key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ull;
    key = (key ^ (key >> 27)) * 0x94d049bb133111ebull;
    u32 k = (u32)(key ^ (key >> 31)) & (VBH - 1);
    Vb* e = g_vbh[k];
    for (; e; e = e->next)
        if (e->start == gva && e->len == len) break;

    int valid = !(g_vk_debug_render_flags & 2) && e && e->buf && (e->own || e->frame == vk_submit_number());
    u64 checked_generation = valid ? gpu_mem_generation() : 0;
    if (valid && !gpu_mem_dirty_since(e->gw, gva, len)) {

        e->gw = checked_generation;
        if (e->epoch == g_vk_epoch && (!strict || e->strict_epoch == g_vk_epoch)) {
            use(e, buf, off);
            return 1;
        }

        int w = e->own ? vk_written_since(gmem_ptr(gva, len), len, e->wseen) : -1;
        if (w == 0) {
            e->epoch = g_vk_epoch;
            if (strict) e->strict_epoch = g_vk_epoch;
            use(e, buf, off);
            return 1;
        }

        if (!strict && w < 0 && e->own && e->stable >= 16 && len >= (64u << 10) && g_vk_epoch - e->chk_epoch < 8) {
            e->epoch = g_vk_epoch;
            use(e, buf, off);
            return 1;
        }
        if (!strict && w < 0 && e->own && e->stable >= 32 && len < (64u << 10) && g_vk_epoch - e->chk_epoch < 4) {
            e->epoch = g_vk_epoch;
            use(e, buf, off);
            return 1;
        }
        if (!strict && w < 0 && e->own && e->stable >= 64 && len >= 4096 &&
            g_vk_epoch - e->chk_epoch < (e->stable >= 256 ? 240u : 64u)) {
            if (g_vk_epoch - e->schk < 4 || vk_sample_hash(gva, len) == e->shash) {
                if (g_vk_epoch - e->schk >= 4) e->schk = g_vk_epoch;
                e->epoch = g_vk_epoch;
                use(e, buf, off);
                return 1;
            }
        }
    }
    u32 wnow = vk_ww_now();
    const u8* src = guest_bytes(gva, len);
    if (!src) return 0;
    u64 h = vk_hash(src, len);
    if (!e) {
        e = calloc(1, sizeof *e);
        if (!e) return 0;
        e->start = gva;
        e->len = len;
        e->hash = h ^ 1;
        e->next = g_vbh[k];
        g_vbh[k] = e;
    }
    if (e->hash == h && e->stable < 100000)
        e->stable++;
    else if (e->hash != h) {
        e->hash = h;
        e->stable = 0;
        drop_own(e);
    }
    e->epoch = e->chk_epoch = g_vk_epoch;
    if (strict) e->strict_epoch = g_vk_epoch;
    e->gw = gpu_mem_generation();
    e->wseen = wnow;
    if (e->stable >= 63 && len >= 4096) {
        e->shash = vk_sample_hash(gva, len);
        e->schk = g_vk_epoch;
    }
    if (!e->own && !(e->stable >= 2 && make_own(e, src))) {

        e->buf = VK_NULL_HANDLE;  
        u8* p = vk_stage(len, vk_stage_align(), &e->buf, &e->off);
        if (!p) return 0;
        memcpy(p, src, len);
        if (copy_stats_on()) g_copy_stage += len;
        e->frame = vk_submit_number();
    }
    use(e, buf, off);
    return 1;
}

int vk_buf_get(u64 gva, u64 len, VkBuffer* buf, u64* off) { return buf_get(gva, len, buf, off, 0); }

int vk_buf_get_strict(u64 gva, u64 len, VkBuffer* buf, u64* off) { return buf_get(gva, len, buf, off, 1); }


void vk_buf_frame(void) {
    if (g_vk_present % 60 || g_vk_present < 600) return;
    if (copy_stats_on() && g_vk_present % 600 == 0) {
        LOG("vulkan: buffer bytes over 600 presents: uncached=%llu zero=%llu calls=%llu peak=%llu staged=%llu owned=%llu bound=%llu writeback=%llu/%llu waits=%llu/%llums reused=%llu\n",
            (unsigned long long)g_copy_uncached, (unsigned long long)g_copy_zero, (unsigned long long)g_copy_calls,
            (unsigned long long)g_copy_peak, (unsigned long long)g_copy_stage, (unsigned long long)g_copy_owned,
            (unsigned long long)g_read_bound, (unsigned long long)g_wb_outputs, (unsigned long long)g_wb_bytes,
            (unsigned long long)g_wb_waits, (unsigned long long)(g_wb_wait_ns / 1000000),
            (unsigned long long)g_wb_reuses);
        LOG("vulkan: buffer waits: draw=%llu/%llums read=%llu/%llums write=%llu/%llums signal=%llu/%llums syncwait=%llu/%llums map=%llu/%llums draw_landed=old_frame:%llu changed:%llu composed=%llu+%llu\n",
            (unsigned long long)g_wb_reason[WB_DRAW], (unsigned long long)(g_wb_reason_ns[WB_DRAW] / 1000000),
            (unsigned long long)g_wb_reason[WB_READ], (unsigned long long)(g_wb_reason_ns[WB_READ] / 1000000),
            (unsigned long long)g_wb_reason[WB_WRITE], (unsigned long long)(g_wb_reason_ns[WB_WRITE] / 1000000),
            (unsigned long long)g_wb_reason[WB_SIGNAL], (unsigned long long)(g_wb_reason_ns[WB_SIGNAL] / 1000000),
            (unsigned long long)g_wb_reason[WB_SYNCWAIT], (unsigned long long)(g_wb_reason_ns[WB_SYNCWAIT] / 1000000),
            (unsigned long long)g_wb_reason[WB_MAP], (unsigned long long)(g_wb_reason_ns[WB_MAP] / 1000000),
            (unsigned long long)g_wb_draw_miss[0], (unsigned long long)g_wb_draw_miss[2],
            (unsigned long long)g_wb_draw_miss[1], (unsigned long long)g_wb_draw_miss[3]);
        LOG("vulkan: aliased storage draws=%llu pages=%llu\n", (unsigned long long)g_alias_draws,
            (unsigned long long)g_alias_pages);
        g_copy_uncached = g_copy_stage = g_copy_owned = g_copy_zero = g_copy_calls = g_copy_peak = g_read_bound = 0;
        g_wb_outputs = g_wb_bytes = g_wb_waits = g_wb_wait_ns = g_wb_reuses = 0;
        g_alias_draws = g_alias_pages = 0;
        memset(g_wb_reason, 0, sizeof g_wb_reason);
        memset(g_wb_reason_ns, 0, sizeof g_wb_reason_ns);
        memset(g_wb_draw_miss, 0, sizeof g_wb_draw_miss);
    }
    for (int k = 0; k < VBH; k++)
        for (Vb** pp = &g_vbh[k]; *pp;) {
            Vb* e = *pp;
            if ((i32)(e->used - (g_vk_present - 600)) >= 0) {
                pp = &e->next;
                continue;
            }
            *pp = e->next;
            drop_own(e);
            free(e);
        }
}
