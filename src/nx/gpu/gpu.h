/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_GPU_H
#define SNX_GPU_H
#include "common/cpu/aarch64/cpu.h"

typedef struct {
    u32 regs[8], carry;
    u32 method, incr;  
    u32 idx, np, pi;   
    u32 pc, dpc;       
    u32 steps;         
    int running;       
    int delayed;       
    int slot;          
    int exiting;       
} GpuMme;

typedef struct {
    u32 mode, sub, m, cnt, k;
} PbBatch;
typedef struct GpuChannel {
    u32 id;
    u32 sub_class[8];
    u32 puller[0x40];
    u32 r3d[0x1000];  
    u32 r3d_shadow[0xE00];  
    u32 r3d_shadow_mode;
    u64 r3d_counter_base[32];  
    u32 r3d_counter_reset_mask;
    u32 rcomp[0x1000];
    u32 r2d[0x400];
    u32 rdma[0x200];
    u32 ri2m[0x80];

    u8* i2m_buf;
    u32 i2m_buf_cap, i2m_len, i2m_pos, i2m_w, i2m_lines, i2m_pitch, i2m_bh;
    u64 i2m_addr;
    int i2m_linear;
    u32 i2m_bd, i2m_dw, i2m_dh, i2m_x, i2m_y, i2m_z;

    PbBatch pb;
    u32 mme_code[0x2000];
    u32 mme_start[0x100];
    u32 mme_upload_ptr, mme_start_ptr;
    u32 mme_params[0x10000];
    u32 mme_nparams;
    int mme_active;
    u32 mme_index;

    u64 cb_addr;
    u32 cb_n;
    u32 cb_words[256];

    u64 vk_cb[5][18];
    u32 vk_cbsize[5][18];

    int render_off;

    u64 cbind[5][18];
    u32 cbind_size[5][18];
    u32 instance_count;  
    u64 dma_job_id;
    u32 dma_launch_index;
    u32* inline_indices;  
    u32 inline_n, inline_cap;
    GpuMme mme;          

    u32 park;       
    u32 park_val;   
    int park_geq;   
    u64 park_addr;  
    u64 park_end;   
} GpuChannel;
enum {
    GPU_PARK_SEMAPHORE = 1,  
    GPU_PARK_REPORT,         
    GPU_PARK_SYNCPOINT,      
    GPU_PARK_FENCE           
};
typedef struct {
    GpuChannel* chan[128];  
    int nchan;
} GpuState;
extern GpuState g_gpu;

typedef struct {
    u32 format, linear, bh, depth, layer, pitch, width, height;
    u64 addr;
} Surf2D;

typedef struct VicPlane {
    u8* p;    
    u64 lim;  
    u32 pitch, h, blk, bh;
} VicPlane;
typedef struct VicSlot {
    int on;  
    u32 sx0, sy0, dx0, dy0, w, h;  
    int mat[3][4], mat_shift, mat_enable;  
    u32 alpha, clamp_lo, clamp_hi;  
    VicPlane iy, ic;
} VicSlot;
typedef struct VicJob {
    u32 fmt, sw, sh;
    VicPlane oy, oc;  
    VicSlot slots[8];
} VicJob;
int gmem_read(u64 gva, void* dst, u64 size);

void gmem_need(u64 gva, u64 size);
void gmem_read_raw(u64 gva, void* dst, u64 size);
void gmem_before_write(u64 gva, u64 size);
void gmem_write_raw(u64 gva, const void* src, u64 size);
void gmem_read_code(u64 gva, void* dst, u64 size);  
int gmem_write(u64 gva, const void* src, u64 size);
u8* gmem_ptr(u64 gva, u64 size);

u8* gmem_ptr_untracked(u64 gva, u64 size);
u32 gmem_r32(u64 gva);
void gmem_w32(u64 gva, u32 v);
u64 bl_offset(u32 x_bytes, u32 y, u32 z, u32 width_bytes, u32 height, u32 bh_log2, u32 bd_log2);
void deswizzle(u8* dst, u32 dst_pitch, const u8* src, u32 w_bytes, u32 h, u32 bh_log2);
void swizzle(u8* dst, const u8* src, u32 src_pitch, u32 w_bytes, u32 h, u32 bh_log2);
void i2m_launch(GpuChannel* ch, u32* regs, int base);
void i2m_data(GpuChannel* ch, u32 v);
void maxwell3d_method(GpuChannel* ch, u32 m, u32 v, int last);
void compute_method(GpuChannel* ch, u32 m, u32 v, int last);
void kepler_i2m_method(GpuChannel* ch, u32 m, u32 v);
void fermi2d_method(GpuChannel* ch, u32 m, u32 v);
void dma_method(GpuChannel* ch, u32 m, u32 v);

void dma_snap_launch(u32 channel, u64 job_id, u32 launch_index, const u32* r, u32 v);
void dma_snap_finish(u32 channel, u64 job_id);
void dma_snap_reset(u32 channel);                                  
void gpu_mem_written(u64 gva, u64 size);
void gpu_mem_written_shader(u64 gva, u64 size);  
void gpu_signal_point(void);                     
u32 gpu_color_bpp(u32 format);  


void gpu_init(void);

void gpu_submit(const void* entries, u32 n, u32 channel, u32 incs, u32 wait_id, u32 wait_val, u32 expect);

void gpu_call(void (*fn)(void*), void* arg, const u32* chans, u32 nchans);
void gpu_channel_close(u32 channel);  
void gpu_release_retire(u32 channel);  

void gpu_park(GpuChannel* ch, u32 kind, u64 addr, u32 val, int geq);

void gpu_kick(void);

void gpu_log_progress(void);
extern int g_gpu_force_visible;  
extern volatile u64 g_gpu_submits;

void gpu_mem_changed(u64 cpu_addr, u64 len);
extern void (*g_gpu_mem_changed)(u64 cpu_addr, u64 len);  

extern void (*g_gpu_before_write)(u64 gva, u64 size);
extern int g_gpu_releases;  

void gpu_release_write(u64 gva, const void* data, u32 len);

extern void (*g_gpu_mapping_begin)(u64 gva, u64 size, int full);
extern void (*g_gpu_mapping_end)(void);


u64 gpu_ticks(void);

void mme_resume(GpuChannel* ch);
void r3d_pending_flush(void);  
int r3d_pending_on(void);


void r3d_clear(GpuChannel* ch, u32 v);

static inline int r3d_clear_scissored(const u32* r) { return (r[0x43E] & 0x100) && (r[0x380] & 1); }
void r3d_draw_texture(GpuChannel* ch);  
void r3d_draw(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 base_vertex, u32 base_instance,
              u32 instances);
void r3d_cb_bind(GpuChannel* ch, int stage, u32 v);  
void r3d_const_upload(GpuChannel* ch, u32 v, int last);
void r3d_const_flush(GpuChannel* ch);  
int r3d_blit(const Surf2D* s, const Surf2D* d, int dx, int dy, int dw, int dh, double sx, double sy, double sdx,
             double sdy);
u64 gpu_mem_generation(void);  
int gpu_mem_dirty_since(u64 since, u64 a, u64 sz);
#endif
