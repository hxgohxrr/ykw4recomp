/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_RENDERER_H
#define SNX_RENDERER_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/gpu/gpu.h"

typedef struct Renderer {
    const char* name;
    int (*start)(void);  

    void (*mem_may_have_changed)(void);

    void (*clear)(GpuChannel* ch, u32 v);
    void (*draw)(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 base_vertex, u32 base_instance,
                 u32 instances);

    void (*method_barrier)(int kind);

    void (*occlusion_reset)(u32 channel);
    void (*occlusion_mark)(u32 channel, u64 address);
    void (*occlusion_invalidate)(u64 address, u32 size);

    void (*draw_texture)(GpuChannel* ch);

    int (*blit)(const Surf2D* s, const Surf2D* d, int dx, int dy, int dw, int dh, double sx, double sy, double sdx,
                double sdy);

    int (*vic_compose)(const VicJob* j);
    void (*rt_flush)(u64 gva);   
    void (*rt_reload)(u64 gva);  
    void (*dispatch)(GpuChannel* ch, const u32* q, u64 code);  

    void (*cs_mem_need)(u64 a, u64 n);
    void (*gfx_mem_need)(u64 a, u64 n);
    void (*before_syncwait)(void);  
    void (*stream_incr)(u32 id);    

    void (*release_write)(u64 gva, const void* data, u32 len);

    void (*release_need)(u64 a, u64 n);
    u32 (*release_value)(u32 id);  
    void (*release_retire)(u32 id);  



    void (*present_buffer)(u64 cpu_addr, u32 w, u32 h, u32 pitch, u32 layout, u32 bhl, u32 fmt, const i32 crop[4],
                           u32 transform);
    u64 (*screenshot)(const char* path);  

    void (*refresh)(void);

    void (*save)(void);

    int (*release_window)(void);
} Renderer;

extern const Renderer vk_renderer;

extern const Renderer* g_renderer;


extern int g_renderer_up;  
extern u64 g_draws;
extern _Atomic int g_cs_any;       
extern _Atomic int g_gfx_any;      
extern _Atomic int g_gfx_img_any;  

#endif
