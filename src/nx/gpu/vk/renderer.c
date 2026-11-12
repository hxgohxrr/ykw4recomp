/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/runtime/frame_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nx/runtime/main.h"
#include "nx/runtime/options.h"
#include "nx/gpu/gpu.h"
#include "nx/hle/services/nvdrv.h"
#include "nx/runtime/platform/backend.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include "nx/gpu/vk/vk.h"
#include "nx/gpu/renderer.h"

static VkSemaphore g_acquired[VK_FRAMES];

static Surf* g_cpu_img;
static Surf* g_last;
static u32 g_last_x, g_last_y, g_last_w, g_last_h;
static u32 g_last_transform;
u32 g_vk_debug_render_flags;
static void vkr_before_write(u64 gva, u64 size) {
    if (size) vk_release_need(gva, size);
    vk_buf_before_write(gva, size);
    if (size)
        vk_gfx_image_mem_need(gva, size);
    else
        vk_gfx_image_land();
    vk_compute_before_write(gva, size);
    vk_rt_before_write(gva, size);
}
static void vkr_gfx_mem_need(u64 gva, u64 size) {
    vk_buf_mem_need(gva, size);
    vk_gfx_image_mem_need(gva, size);
}
static void vkr_mapping_begin(u64 gva, u64 size, int full) {
    vk_lock();
    vk_release_drain();
    if (full) {
        vk_buf_land_map();
        vk_gfx_image_land();
        vk_compute_land();
    } else {
        vk_buf_mem_need_map(gva, size);
        vk_gfx_image_mem_need(gva, size);
        vk_compute_mem_need_map(gva, size);
    }
}
static void vkr_mapping_end(void) { vk_unlock(); }


static int vkr_start(void) {
    if (!vk_init()) return 0;
    LOG("vulkan: synchronization gpu_sync_trace=%d\n", g_opt_gpu_sync_trace);
    LOG("vulkan: visibility force_visible=%d backface_culling=%d\n",
        g_gpu_force_visible, g_opt_backface_culling);
    g_vk_scale = g_opt_resf;        
    g_vk_anisotropy = g_opt_aniso;  
    if (g_vk_anisotropy > 1 && !g_vk.feat.samplerAnisotropy)
        LOG("vulkan: this GPU has no anisotropic filtering, so anisotropy=%d does nothing\n", g_vk_anisotropy);
    if (g_vk_scale != 1.0f) LOG("vulkan: render targets at %.2fx\n", g_vk_scale);
    vk_cmd_init();
    vk_pipeline_init();
    g_gpu_before_write = vkr_before_write;
    g_gpu_mapping_begin = vkr_mapping_begin;
    g_gpu_mapping_end = vkr_mapping_end;
    g_gpu_mem_changed = vk_mem_changed;
    VkSemaphoreCreateInfo sci = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (!g_opt_headless)
        for (int i = 0; i < VK_FRAMES; i++)
            if (!VK_OK(vkCreateSemaphore(g_vk.dev, &sci, NULL, &g_acquired[i]))) return 0;
    g_renderer_up = 1;  
    return 1;
}



static void vkr_mem_may_have_changed(void) { __atomic_add_fetch(&g_vk_epoch, 1, __ATOMIC_RELAXED); }
static void vkr_occlusion_reset(u32 channel) {
    vk_lock();
    vk_occlusion_reset(channel);
    vk_unlock();
}
static void vkr_occlusion_mark(u32 channel, u64 address) {
    vk_lock();
    vk_occlusion_mark(channel, address);
    vk_unlock();
}
static void vkr_occlusion_invalidate(u64 address, u32 size) {
    vk_lock();
    vk_occlusion_invalidate(address, size);
    vk_unlock();
}
static void vkr_draw_texture(GpuChannel* ch) {
    vk_lock();
    vk_render_draw_texture(ch);
    vk_unlock();
}
static void vkr_clear(GpuChannel* ch, u32 v) {
    vk_lock();
    vk_render_clear(ch, v);
    vk_unlock();
}
static void vkr_draw(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 bv, u32 bi, u32 instances) {
    g_draws++;
    u64 timing = frame_profile_begin();
    vk_lock();
    vk_render_draw(ch, indexed, topology, first, count, bv, bi, instances);
    vk_unlock();
    frame_profile_end(FP_DRAW_CPU, timing);
}
static void vkr_method_barrier(int kind) {
    vk_lock();
    if (kind == 3) {

        vk_tex_invalidate_all();
        __atomic_add_fetch(&g_vk_epoch, 1, __ATOMIC_RELAXED);
    } else {
        vk_render_end();

        VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                               .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
        VkDependencyInfo di = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &mb};
        vkCmdPipelineBarrier2(vk_cmd(), &di);
        if (kind == 2) {

            vk_tex_invalidate_all();
            __atomic_add_fetch(&g_vk_epoch, 1, __ATOMIC_RELAXED);
        }
    }
    vk_unlock();
}
static int vkr_blit(const Surf2D* s, const Surf2D* d, int dx, int dy, int dw, int dh, double sx, double sy, double sdx,
                    double sdy) {
    vk_lock();
    int r = vk_blit2d(s, d, dx, dy, dw, dh, sx, sy, sdx, sdy);
    vk_unlock();
    return r;  
}
static int vkr_vic_compose(const VicJob* j) {
    static int off = -1;
    if (off < 0) off = getenv("SNX_NO_VIC_GPU") != NULL;
    if (off) return 0;
    return vk_vic_compose(j);  
}
static void vkr_rt_flush(u64 gva) {
    vk_lock();
    vk_gfx_image_mem_need(gva, 1);
    vk_rt_flush(gva);
    vk_unlock();
}
static void vkr_rt_reload(u64 gva) {
    vk_lock();
    vk_rt_reload(gva);
    vk_unlock();
}
static void vkr_dispatch(GpuChannel* ch, const u32* q, u64 code) {
    static int off = -1;
    if (off < 0) off = getenv("SNX_NO_COMPUTE") != NULL;
    if (off) return;
    u64 timing = frame_profile_begin();
    r3d_pending_flush();  
    vk_lock();
    vk_dispatch(ch, q, code);
    vk_unlock();
    frame_profile_end(FP_COMPUTE_CPU, timing);
}

static void vkr_before_syncwait(void) {
    vk_buf_land_syncwait();
    vk_gfx_image_land();
    vk_compute_land();
}
static void vkr_stream_incr(u32 id) { vk_release_incr(id); }



static Surf* cpu_frame(u64 cpu_addr, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt) {
    if (g_cpu_img && (g_cpu_img->w != w || g_cpu_img->h != h)) {
        vk_defer_image(g_cpu_img->img, g_cpu_img->mem, VK_NULL_HANDLE, g_cpu_img->att);
        if (g_last == g_cpu_img) g_last = NULL;
        free(g_cpu_img);
        g_cpu_img = NULL;
    }
    if (!g_cpu_img) {
        g_cpu_img = calloc(1, sizeof *g_cpu_img);
        if (!g_cpu_img) return NULL;
        g_cpu_img->w = g_cpu_img->sw = w;
        g_cpu_img->h = g_cpu_img->sh = h;
        g_cpu_img->f = vk_rt_format(0xD5);  
        g_cpu_img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                .imageType = VK_IMAGE_TYPE_2D,
                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .extent = {w, h, 1},
                                .mipLevels = 1,
                                .arrayLayers = 1,
                                .samples = VK_SAMPLE_COUNT_1_BIT,
                                .tiling = VK_IMAGE_TILING_OPTIMAL,
                                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
        VmaAllocationCreateInfo aci = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        if (!VK_OK(vmaCreateImage(g_vk.vma, &ii, &aci, &g_cpu_img->img, &g_cpu_img->mem, NULL))) {
            free(g_cpu_img);
            g_cpu_img = NULL;
            return NULL;
        }
    }
    VkBuffer buf;
    u64 off;
    u32* px = (u32*)vk_stage((u64)w * h * 4, 4, &buf, &off);
    if (!px) return NULL;
    vi_convert_cpu(cpu_addr, w, h, pitch, layout, bhl, fmt, px);
    VkCommandBuffer cb = vk_cmd();
    vk_surf_use(cb, g_cpu_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy c = {
        .bufferOffset = off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {w, h, 1}};
    vkCmdCopyBufferToImage(cb, buf, g_cpu_img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
    return g_cpu_img;
}

static int is_srgb(VkFormat f) {
    return f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}

static int g_sc_srgb;  

static u32 g_generation;
static int g_lost;
static u64 g_shown_ns;  

static void present_timing(void) {
    static int initialized;
    static FILE* csv;
    static u64 first, previous, flushed;
    if (!initialized) {
        initialized = 1;
        const char* path = getenv("SNX_PRESENT_CSV");
        if (path && *path) {
            csv = fopen(path, "w");
            if (csv) {
                setvbuf(csv, NULL, _IOFBF, 65536);
                fputs("present,elapsed_ns,interval_ns,draws,finishes,finish_ns\n", csv);
            }
        }
    }
    if (!csv) return;
    u64 now = plat_ns();
    if (!first) first = previous = flushed = now;
    fprintf(csv, "%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)g_vk_present, (unsigned long long)(now - first),
            (unsigned long long)(now - previous), (unsigned long long)g_vk_drawn,
            (unsigned long long)g_vk_finishes, (unsigned long long)g_vk_finish_ns);
    previous = now;
    if (now - flushed >= 1000000000ull) {
        fflush(csv);
        flushed = now;
    }
}

static void flip_src(VkImageBlit* b, u32 transform) {
    if (transform & 1) {
        i32 x = b->srcOffsets[0].x;
        b->srcOffsets[0].x = b->srcOffsets[1].x;
        b->srcOffsets[1].x = x;
    }
    if (transform & 2) {
        i32 y = b->srcOffsets[0].y;
        b->srcOffsets[0].y = b->srcOffsets[1].y;
        b->srcOffsets[1].y = y;
    }
}


static void show(Surf* src, u32 fw, u32 fh, u32 transform, int game) {
    static int diagnostic_flags = -1;
    if (diagnostic_flags < 0) diagnostic_flags = getenv("SNX_DIAG_RENDER_FLAGS") != NULL;
    if (game && diagnostic_flags) {
        vk_debug_draw_trace_end();
        char path[1400];
        snprintf(path, sizeof path, "%scaptures/render-flags.txt", g_exedir);
        FILE* file = fopen(path, "rb");
        unsigned flags = 0;
        if (file) { if (fscanf(file, "%x", &flags) != 1) flags = 0; fclose(file); }
        if (flags != g_vk_debug_render_flags) {
            LOG("vulkan: diagnostic render flags %x\n", flags);
            if (flags & 16) g_vk_debug_trace = 1;
        }
        g_vk_debug_render_flags = flags;
    }
    if (game && g_opt_diag) {
        vk_debug_draw_trace_end();
        vk_debug_surface_snapshot();
    }

    int ww, wh, srgb = src ? is_srgb(src->f.vk) : g_sc_srgb, shown = backend_window_shown();
    backend_window_size(&ww, &wh);
    if (shown && (g_lost || backend_window_generation() != g_generation)) {
        vk_finish();
        g_lost = !vk_surface_recreate();
        g_generation = backend_window_generation();
    }
    shown = shown && !g_lost;
    int tried = 0;  

    if (shown && ww > 0 && wh > 0 &&
        (ww != g_vk.sc_win_w || wh != g_vk.sc_win_h || srgb != g_sc_srgb ||
         __atomic_load_n(&g_opt_vsync, __ATOMIC_RELAXED) != g_vk.sc_vsync)) {
        vk_finish();
        g_sc_srgb = srgb;
        vk_swapchain_create(srgb);
        tried = 1;
    }
    VkSemaphore wait = VK_NULL_HANDLE, signal = VK_NULL_HANDLE;
    u32 idx = 0;
    int recreate = 0;
    if (shown && (src || !game) && (g_vk.swapchain || (!tried && vk_swapchain_create(srgb)))) {
        VkCommandBuffer cb = vk_cmd();  
        VkSemaphore acq = g_acquired[vk_frame_id() % VK_FRAMES];

        VkResult r = vkAcquireNextImageKHR(g_vk.dev, g_vk.swapchain, 0, acq, VK_NULL_HANDLE, &idx);
        if (r == VK_NOT_READY || r == VK_TIMEOUT) {

            static u64 dropped;
            if (game && ++dropped % 600 == 1)
                LOG("vulkan: %llu frames not shown, the display had every swapchain image\n",
                    (unsigned long long)dropped);
        } else if (r == VK_ERROR_OUT_OF_DATE_KHR)
            recreate = 1;
        else if (r == VK_ERROR_SURFACE_LOST_KHR)
            g_lost = 1;
        else if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) {
            wait = acq;
            signal = g_vk.sc_done[idx];
            VkImage sc = g_vk.sc_images[idx];
            if (src)
                vk_surf_use(cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                            VK_ACCESS_2_TRANSFER_READ_BIT);

            vk_barrier(cb, sc, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkClearColorValue black = {{0, 0, 0, 1}};
            VkImageSubresourceRange all = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdClearColorImage(cb, sc, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &all);
            vk_barrier(cb, sc, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                       VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            u32 sw = g_vk.sc_extent.width, sh = g_vk.sc_extent.height, cx = 0, cy = 0, cw = sw, ch = sh, dw, dh;
            int sx, sy, sww, swh;
            backend_window_safe(&sx, &sy, &sww, &swh);
            if (g_vk.sc_win_w > 0 && g_vk.sc_win_h > 0 && sww > 0 && swh > 0 && sx + sww <= g_vk.sc_win_w &&
                sy + swh <= g_vk.sc_win_h) {
                cx = (u32)((u64)sw * (u32)sx / (u32)g_vk.sc_win_w);
                cy = (u32)((u64)sh * (u32)sy / (u32)g_vk.sc_win_h);
                cw = (u32)((u64)sw * (u32)sww / (u32)g_vk.sc_win_w);
                ch = (u32)((u64)sh * (u32)swh / (u32)g_vk.sc_win_h);
            }
            if (src && fw && fh) {
                dw = cw;
                dh = (u32)((u64)cw * fh / fw);
                if (dh > ch) {
                    dh = ch;
                    dw = (u32)((u64)ch * fw / fh);
                }
                i32 dx = (i32)(cx + (cw - dw) / 2), dy = (i32)(cy + (ch - dh) / 2);
                VkImageBlit blit = {.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                    .srcOffsets = {{(i32)g_last_x, (i32)g_last_y, 0},
                                                   {(i32)(g_last_x + g_last_w), (i32)(g_last_y + g_last_h), 1}},
                                    .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                    .dstOffsets = {{dx, dy, 0}, {dx + (i32)dw, dy + (i32)dh, 1}}};
                flip_src(&blit, transform);  
                vkCmdBlitImage(cb, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sc,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            }
            {
                int safe[4] = {(int)cx, (int)cy, (int)cw, (int)ch};
                vk_ui_draw(cb, sc, safe);
            }
            vk_barrier(cb, sc, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
                       VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        } else
            VK_OK(r);
    }
    if (game) {
        g_vk_present++;
        if (g_last) g_last->last_used = g_vk_present;  
        vk_tex_frame();
        vk_buf_frame();
        if (g_vk_present % 600 == 0) {
            LOG("vulkan: descriptor sets %llu written, %llu reused\n", g_vk_dset_new, g_vk_dset_reused);
            LOG("vulkan: %llu draws, %llu skipped; %llu dispatches in %llu batches (%llu ms waiting), %llu skipped; "
                "%llu finishes (%llu ms)\n",
                (unsigned long long)g_vk_drawn, (unsigned long long)g_vk_skipped, (unsigned long long)g_vk_dispatches,
                (unsigned long long)g_vk_cs_batches, (unsigned long long)(__atomic_load_n(&g_vk_cs_wait_ns, __ATOMIC_RELAXED) / 1000000),
                (unsigned long long)g_vk_cs_skipped, (unsigned long long)g_vk_finishes,
                (unsigned long long)(g_vk_finish_ns / 1000000));
            if (g_gpu_force_visible == 2) LOG("vulkan: occlusion predicate copies %llu batches, %llu calls\n", g_vk_occ_copy_batches, g_vk_occ_copy_calls);
            if (g_gpu_force_visible == 2)
                LOG("vulkan: conservative occlusion %llu queries, %llu guarded draws, %llu uncertain (visible)\n",
                    (unsigned long long)g_vk_occ_queries, (unsigned long long)g_vk_occ_guards,
                    (unsigned long long)g_vk_occ_fallback);
            if (g_vk_occ_checked_draws)
                LOG("vulkan: retired occlusion predicates hid %llu of %llu checked draws (diagnostic only)\n",
                    (unsigned long long)g_vk_occ_hidden_draws, (unsigned long long)g_vk_occ_checked_draws);
        }
    }

    vk_submit(wait, VK_PIPELINE_STAGE_2_CLEAR_BIT, signal);
    if (game && g_opt_gpu_sync_trace == 2) {
        LOG_LIMIT(1, "vulkan: experimental frame-completion waits enabled (gpu_sync_trace=2)\n");
        vk_compute_land();
        vk_finish();
    }
    vk_frame_next();  
    if (signal) {
        VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &signal,
                               .swapchainCount = 1,
                               .pSwapchains = &g_vk.swapchain,
                               .pImageIndices = &idx};
        u64 timing = frame_profile_begin();
        VkResult r = vkQueuePresentKHR(g_vk.gfx, &pi);
        frame_profile_end(FP_PRESENT_CPU, timing);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || (r == VK_SUBOPTIMAL_KHR && !g_vk.sc_turned))
            recreate = 1;
        else if (r == VK_ERROR_SURFACE_LOST_KHR)
            g_lost = 1;
        else
            VK_OK(r);
    }
    if (recreate && !g_lost && !g_vk.sc_lost) {  
        vk_finish();
        vk_swapchain_create(srgb);
    }
    if (game) present_timing();
    if (g_vk.sc_lost) {  
        g_vk.sc_lost = 0;
        g_lost = 1;
    }
}


static void vkr_present_buffer(u64 cpu_addr, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, const i32 crop[4],
                               u32 transform) {
    vk_lock();
    vk_buf_writable_abort();  
    vk_render_end();
    Surf* src = NULL;
    if (w && h && w <= g_vk.props.limits.maxImageDimension2D && h <= g_vk.props.limits.maxImageDimension2D) {
        src = vk_rt_for_cpu(cpu_addr, w, h);
        if (!src || src->f.is_int) src = cpu_frame(cpu_addr, w, h, pitch, layout, bhl, fmt);
    }

    u32 x0 = 0, y0 = 0, x1 = w, y1 = h;
    if (crop[2] > crop[0] && crop[3] > crop[1] && crop[0] >= 0 && crop[1] >= 0) {
        x0 = (u32)crop[0] < w ? (u32)crop[0] : 0;
        y0 = (u32)crop[1] < h ? (u32)crop[1] : 0;
        x1 = (u32)crop[2] < w ? (u32)crop[2] : w;
        y1 = (u32)crop[3] < h ? (u32)crop[3] : h;
    }
    if (src) {  
        if (x1 > src->w) x1 = src->w;
        if (y1 > src->h) y1 = src->h;
        if (x1 <= x0 || y1 <= y0) {
            x0 = y0 = 0;
            x1 = w < src->w ? w : src->w;
            y1 = h < src->h ? h : src->h;
        }
    }
    if (src) {  
        g_last = src;
        g_last_transform = transform;
        g_last_x = (u32)((u64)src->sw * x0 / src->w);
        g_last_y = (u32)((u64)src->sh * y0 / src->h);
        g_last_w = (u32)((u64)src->sw * x1 / src->w) - g_last_x;
        g_last_h = (u32)((u64)src->sh * y1 / src->h) - g_last_y;
    }
    show(src, x1 - x0, y1 - y0, transform, 1);
    __atomic_store_n(&g_shown_ns, plat_ns(), __ATOMIC_RELAXED);
    vk_unlock();
}


static void vkr_refresh(void) {
    static u64 last;
    u64 now = plat_ns();
    if (!backend_window_shown() || now - __atomic_load_n(&g_shown_ns, __ATOMIC_RELAXED) < 250000000ull ||
        now - last < 33000000ull)
        return;
    if (!vk_trylock()) return;
    last = now;
    vk_render_end();
    show(g_last, g_last_w, g_last_h, g_last_transform, 0);
    vk_unlock();
}


static u64 surf_save(Surf* s, u32 x, u32 y, u32 w, u32 h, u32 transform, const char* path) {
    u64 hash = 1469598103934665603ull;

    VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = is_srgb(s->f.vk) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                            .extent = {w, h, 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo aci = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkImage tmp;
    VmaAllocation tmem;
    if (!VK_OK(vmaCreateImage(g_vk.vma, &ii, &aci, &tmp, &tmem, NULL))) return hash;
    VkBuffer buf;
    u8* p = vk_readback_buffer((u64)w * h * 4, &buf);
    if (!p) {
        vmaDestroyImage(g_vk.vma, tmp, tmem);
        return hash;
    }
    vk_render_end();  
    VkCommandBuffer cb = vk_cmd();
    vk_surf_use(cb, s, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT);
    vk_barrier(cb, tmp, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageBlit blit = {.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .srcOffsets = {{(i32)x, (i32)y, 0}, {(i32)(x + w), (i32)(y + h), 1}},
                        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .dstOffsets = {{0, 0, 0}, {(i32)w, (i32)h, 1}}};
    flip_src(&blit, transform);  
    vkCmdBlitImage(cb, s->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tmp, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &blit, VK_FILTER_NEAREST);
    vk_barrier(cb, tmp, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy c = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {w, h, 1}};
    vkCmdCopyImageToBuffer(cb, tmp, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &c);
    VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                           .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                           .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                           .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &mb};
    vkCmdPipelineBarrier2(cb, &di);
    vk_finish();
    vk_readback_ready();
    vmaDestroyImage(g_vk.vma, tmp, tmem);
    const u32* px = (const u32*)p;
    for (u64 i = 0; i < (u64)w * h; i++) hash = (hash ^ px[i]) * 1099511628211ull;
    if (path) {
        char fn[1100];
        snprintf(fn, sizeof fn, "%s.ppm", path);
        FILE* f = fopen(fn, "wb");
        int ok = f != NULL;
        if (f) {
            fprintf(f, "P6\n%u %u\n255\n", w, h);
            for (u64 i = 0; i < (u64)w * h && ok; i++) {
                u8 rgb[3] = {(u8)px[i], (u8)(px[i] >> 8), (u8)(px[i] >> 16)};
                ok = fwrite(rgb, 1, 3, f) == 3;
            }
            ok = fclose(f) == 0 && ok;
        }
        if (!ok) WARN_LIMIT(1, "vulkan: the screenshot %s couldn't be written\n", fn);
    }
    return hash;
}

static u64 vkr_screenshot(const char* path) {
    vk_lock();
    u64 hash = g_last ? surf_save(g_last, g_last_x, g_last_y, g_last_w, g_last_h, g_last_transform, path)
                      : 1469598103934665603ull;
    vk_unlock();
    return hash;
}

const Renderer vk_renderer = {
    .name = "Vulkan",
    .start = vkr_start,
    .mem_may_have_changed = vkr_mem_may_have_changed,
    .clear = vkr_clear,
    .draw_texture = vkr_draw_texture,
    .draw = vkr_draw,
    .method_barrier = vkr_method_barrier,
    .occlusion_reset = vkr_occlusion_reset,
    .occlusion_mark = vkr_occlusion_mark,
    .occlusion_invalidate = vkr_occlusion_invalidate,
    .blit = vkr_blit,
    .vic_compose = vkr_vic_compose,
    .rt_flush = vkr_rt_flush,
    .rt_reload = vkr_rt_reload,
    .dispatch = vkr_dispatch,
    .cs_mem_need = vk_compute_mem_need,
    .gfx_mem_need = vkr_gfx_mem_need,
    .before_syncwait = vkr_before_syncwait,
    .stream_incr = vkr_stream_incr,
    .release_write = vk_release_write,
    .release_need = vk_release_need,
    .release_value = vk_release_value,
    .release_retire = vk_release_retire,
    .present_buffer = vkr_present_buffer,
    .screenshot = vkr_screenshot,
    .refresh = vkr_refresh,
    .save = vk_pipeline_save,
    .release_window = vk_window_release,
};
const Renderer* g_renderer = &vk_renderer;
