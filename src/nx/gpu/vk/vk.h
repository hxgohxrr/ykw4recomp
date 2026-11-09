/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_VK_H
#define SNX_VK_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/gpu/gpu.h"
#include "volk.h"
#include "vk_mem_alloc.h"
#include "nx/gpu/shader/shader.h"

#define VK_FRAMES 2   
#define VK_SC_MAX 16  

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug;
    VkSurfaceKHR surface;
    VkPhysicalDevice phys;
    VkPhysicalDeviceProperties props;
    VkDevice dev;
    VmaAllocator vma;

    u32 gfx_family, compute_family;
    VkQueue gfx, compute;

    VkSwapchainKHR swapchain;
    VkExtent2D sc_extent;
    int sc_win_w, sc_win_h;  
    int sc_srgb;             
    VkFormat sc_format;      
    int sc_vsync;   
    int sc_turned;  
    int sc_lost;    
    u32 sc_count;
    VkImage sc_images[VK_SC_MAX];
    VkSemaphore sc_done[VK_SC_MAX];  

    VkPhysicalDeviceFeatures feat;
    int has_divisor;       
    int has_border_color;  
    u32 border_color_max;  
    int has_mirror_clamp;  
    int has_shader_draw_parameters;
    int has_conditional_rendering;
} VkState;
extern VkState g_vk;


int vk_init(void);

int vk_swapchain_create(int srgb);
int vk_surface_recreate(void);
int vk_window_release(void);  

#define VK_OK(call) vk_ok_((call), #call, __FILE__, __LINE__)
int vk_ok_(VkResult r, const char* what, const char* file, int line);
void vk_report_device_fault(void);  
int vk_device_fault_details(PFN_vkGetDeviceFaultInfoEXT read, VkDevice device);
void vk_lock(void);
void vk_unlock(void);
int vk_trylock(void);  


void vk_cmd_init(void);
VkCommandBuffer vk_cmd(void);  

u8* vk_stage(u64 size, u64 align, VkBuffer* buf, u64* off);

u8* vk_stage_output(u64 size, u64 align, VkBuffer* buf, u64* off, VmaAllocation* mem);

u8* vk_stage_back(u64 size, VkBuffer* buf, u64* off, VmaAllocation* mem);

void vk_submit(VkSemaphore wait, VkPipelineStageFlags2 wait_stage, VkSemaphore signal);
void vk_graphics_dependency(void);  
void vk_finish(void);  
extern u64 g_vk_finishes, g_vk_finish_ns;  
void vk_frame_next(void);                  
void vk_frame_check(void);                 
u64 vk_completed_submit(void);
#define VK_OCC_BUDGET 8
typedef struct { VkQueryPool pool; u32 query, gpu_query, channel; u64 generation; int safe32; } VkOccTicket;
extern u64 g_vk_occ_copy_batches, g_vk_occ_copy_calls;
void vk_occlusion_reset(u32 channel);
int vk_occlusion_report(u32 channel, u64* samples);
VkOccTicket vk_occlusion_prepare(u32 channel, int safe32, int proxy);
void vk_occlusion_mark(u32 channel, u64 address);
void vk_occlusion_invalidate(u64 address, u32 size);
int vk_occlusion_condition(const GpuChannel* ch, VkConditionalRenderingBeginInfoEXT* condition);
void vk_occlusion_begin(VkCommandBuffer cb, VkOccTicket* ticket);
void vk_occlusion_end(VkCommandBuffer cb, const VkOccTicket* ticket);
extern u64 g_vk_occ_ready, g_vk_occ_fallback, g_vk_occ_queries;
extern u64 g_vk_occ_guards;
extern u64 g_vk_occ_checked_draws, g_vk_occ_hidden_draws;

u8* vk_readback_buffer(u64 size, VkBuffer* buf);
void vk_readback_ready(void);

void vk_defer_image(VkImage img, VmaAllocation a, VkImageView v1, VkImageView v2);
void vk_defer_buffer(VkBuffer b, VmaAllocation a);
void vk_defer_buffer_view(VkBufferView v);

u64 vk_submit_number(void);
u32 vk_cb_begun(void);  
u64 vk_frame_id(void);
u64 vk_last_submit(void);         
void vk_wait_submit(u64 value);   
void vk_land_through(u64 value);  
void vk_barrier(VkCommandBuffer cb, VkImage img, VkImageAspectFlags aspect, VkPipelineStageFlags2 src_stage,
                VkAccessFlags2 src_access, VkImageLayout from, VkPipelineStageFlags2 dst_stage,
                VkAccessFlags2 dst_access, VkImageLayout to);

VkDescriptorSet vk_desc_alloc(VkDescriptorSetLayout layout);

#define VK_POOL_TYPES 5  
void vk_pool_sizes(VkDescriptorPoolSize* sz, u32 sets);
extern u32 g_vk_buffer_image_slots;  

u64 vk_stage_align(void);
u64 vk_hash(const void* p, u64 size);

u64 vk_sample_hash(u64 gva, u64 len);

extern u32 g_vk_epoch, g_vk_present;


typedef struct {
    VkFormat vk;
    u8 bpp;  
    u8 bw, bh;
    u8 depth, stencil, is_int;
    u8 raw;   
    u8 sint;  
    u8 conv;  
    u8 bgra;  
} VkFmt;
enum {
    VKC_NONE,
    VKC_ASTC,
    VKC_RGBA4,
    VKC_RGB5A1,
    VKC_RGB32F,
    VKC_A5B5G5R1,
    VKC_D24S8,
    VKC_S8D24,
    VKC_D24X8,
    VKC_D32S8,

    VKC_ETC2,
    VKC_ETC2_PTA,
    VKC_ETC2_RGBA,
    VKC_EAC_R,
    VKC_EAC_R_SNORM,
    VKC_EAC_RG,
    VKC_EAC_RG_SNORM
};
VkFmt vk_rt_format(u32 fmt);
VkFmt vk_zt_format(u32 fmt);
VkFmt vk_tic_format(u32 w0, int srgb);  
void vk_depth_unpack(const VkFmt* f, const u8* src, u64 pixels, u8* depth, u8* stencil);
int vk_tic_reads_stencil(const u32* tic);

VkFormat vk_srgb_as(VkFormat f, int srgb);
typedef struct {
    VkImageFormatListCreateInfo list;
    VkFormat f[2];
} VkViewFormats;

void vk_view_formats(VkImageCreateInfo* ii, VkViewFormats* vf);


typedef struct {
    u64 key;  
    VkImageView v;
} SurfView;
typedef struct Surf {
    u64 gva;
    u32 w, h, layers, levels;
    u32 sw, sh;   
    u32 depth3d;  
    u32 fkey;     
    VkFmt f;
    u32 bl, bh, bd, pitch;  
    u64 layer_size;         
    int is_rt;
    int storage_capable;   
    int volume;            
    VkImageViewType type;  
    VkImage img;
    VmaAllocation mem;
    VkImageView att;       
    VkImageView att_linear;  
    VkImageLayout layout;  
    VkPipelineStageFlags2 stage;
    VkAccessFlags2 access;
    u64 last_used;
    u32 wgen;        
    u32 drawn_at;    
    u64 draw_gw;     
    u64 draw_hash;   
    u32 draw_wseen;  
    int draw_has_hash;
    int feeds_tex;  

    struct {
        struct Surf* t;
        u32 wgen, tgen;
    } copied[2];
    u32 mem_wgen;  
    u64 gw;  

    SurfView sv[6];
    int nsv;
    u64 size, hash, shash;
    int upload_valid;  
    u32 checked, next_check, stable, tfull;
    u32 tex_inval_gen;  
    u32 wseen;  

    struct Surf* crop;
    u32 crop_gen;
    struct Surf* next;
    struct Surf* rt_next;  
} Surf;

extern float g_vk_scale;
u32 vk_sc(u32 v);       
float vk_scf(float v);  

Surf* vk_rt(u64 gva, u32 w, u32 h, u32 fmt, u32 tile, int depth, u32 pitch);
Surf* vk_rt_array(u64 gva, u32 w, u32 h, u32 fmt, u32 tile, u32 pitch, u32 layers, u64 layer_size, int volume);
Surf* vk_zt_array(u64 gva, u32 w, u32 h, u32 fmt, u32 tile, u32 layers, u64 layer_size);
Surf* vk_rt_find(u64 gva);
Surf* vk_rt_snapshot(Surf* rt);                   
Surf* vk_rt_for_cpu(u64 cpu_addr, u32 w, u32 h);  

void vk_rt_note_read(Surf* s);
void vk_rt_before_write(u64 gva, u64 size);

VkImage vk_unscaled_image(const Surf* s, u32 layers, VmaAllocation* mem);

void vk_surf_use(VkCommandBuffer cb, Surf* s, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access);

#define VK_SAMPLED_STAGES                                                                                              \
    (VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
VkImageView vk_rt_attachment(Surf* s, int srgb_write);
void vk_rt_flush(u64 gva);
void vk_gfx_image_mem_need(u64 gva, u64 size);
void vk_gfx_image_land(void);
u64 vk_surf_guest_size(const Surf* s);
int vk_surf_can_writeback(const Surf* s);
void vk_surf_flush(Surf* s, const u8* original);  
void vk_debug_surface_snapshot(void);  
extern int g_vk_debug_trace;
extern u32 g_vk_debug_skip_fs;
extern u32 g_vk_debug_before_fs;
extern u32 g_vk_debug_render_flags;
void vk_debug_draw_trace_end(void);
void vk_tex_note_writeback(Surf* s);
void vk_rt_drawn(Surf* s);       
void vk_rt_drawn_full(Surf* s);  
int vk_rt_drawn_fresh(Surf* s);  
int vk_rt_drawn_in(u64 lo, u64 hi, Surf** out, int max);  
void vk_rt_reload(u64 gva);

Surf* vk_surf_first(u64 gva);
void vk_surf_add(Surf* s);
VkImageAspectFlags vk_aspect(const VkFmt* f);
void vk_surf_evict(u32 before);  
int vk_blit2d(const Surf2D* s, const Surf2D* d, int dx, int dy, int dw, int dh, double sx, double sy, double sdx,
              double sdy);


void vk_astc_upload(VkCommandBuffer cb, Surf* t, const u8* blocks, u32 w, u32 h, u32 depth, u32 level, u32 layer);


int vk_vic_compose(const VicJob* j);

void vk_render_forget_state(void);
extern u64 g_vk_dset_new, g_vk_dset_reused;


Surf* vk_tex_from_tic(const u32* tic);
Surf* vk_tex_from_tic_raw(const u32* tic);  
void vk_tex_invalidate_all(void);           


VkImageView vk_tex_view(Surf* s, VkImageViewType type, int srgb, const VkComponentMapping* map, int stencil);

VkImageView vk_null_view(VkImageViewType type, int num);

VkSampler vk_sampler(const u32* tsc, int raise, int integer);
extern int g_vk_anisotropy;  
void vk_tex_frame(void);     


int vk_buf_get(u64 gva, u64 len, VkBuffer* buf, u64* off);
int vk_buf_get_strict(u64 gva, u64 len, VkBuffer* buf, u64* off);  

int vk_buf_copy(u64 gva, u64 len, VkBuffer* buf, u64* off);
int vk_buf_copy_writable(u64 gva, u32 len, VkBuffer* buf, u64* off);
typedef struct {
    u64 addr;
    u32 size;
    int writable, aliased;
    VkBuffer buf;
    u64 off, range;
} VkDrawGbuf;
typedef struct {
    VkDrawGbuf b[2][16];
    int n[2];
    VkBuffer map_buf[2];
    u64 map_off[2], map_size[2];
} VkDrawStorage;
int vk_buf_draw_has_alias(const VkDrawStorage* draw);
int vk_buf_draw_prepare(VkDrawStorage* draw);
void vk_buf_writable_abort(void);
void vk_buf_writable_commit(void);
void vk_buf_mem_need(u64 gva, u64 len);
int vk_buf_pending(u64 gva, u64 len);  
int vk_buf_pending_any(void);          
int vk_buf_pending_unsubmitted(void);  

void vk_release_write(u64 gva, const void* data, u32 len);
void vk_release_incr(u32 id);
void vk_release_need(u64 gva, u64 len);
u32 vk_release_value(u32 id);
void vk_release_retire(u32 id);
void vk_release_drain(void);
void vk_buf_mem_need_map(u64 gva, u64 len);
void vk_buf_before_write(u64 gva, u64 len);
void vk_buf_land(void);
void vk_buf_land_syncwait(void);
void vk_buf_land_map(void);
void vk_buf_frame_done(u64 frame);
void vk_buf_host_barrier(VkCommandBuffer cb, u64 frame);
void vk_buf_frame(void);

void vk_result_begin(void);
void vk_result_end(void);
int vk_result_landing(void);
void vk_write_changed(u64 addr, const u8* res, const u8* org, u32 size);

int vk_written_since(const u8* host, u64 len, u32 since);
u32 vk_ww_now(void);
void vk_mem_changed(u64 cpu, u64 len);  



typedef struct {
    u32 idx, off, ok, v;
} VkCbRead;
typedef struct {
    VkCbRead* r;
    u32 n;
} VkCbReads;
typedef int (*VkCbReader)(u32 idx, u32 off, u32* out);
u32* vk_translate_spirv(const u8* code, u32 max_bytes, int stage, ShaderInfo* si, size_t* words, VkCbReader read,
                        VkCbReads* reads);

const char* vk_spirv_unavailable(const u32* spv, size_t words);
u64 vk_shader_hash(const u8* code, u32 max_bytes, u32* out_len, VkCbReader read);  

const char* vk_corpus_dir(void);
int vk_cb_reads_hold(const VkCbReads* reads, VkCbReader read);
typedef struct VkSh {
    u64 hash;
    u32 mask[20];  
    int stage;
    VkCbReads cbr;
    ShaderInfo si;
    VkShaderModule mod;
    int failed;
    u32 clen, clen_a;
    u64 qh, qh_a;
    struct VkSh* next;
} VkSh;
extern VkPipelineLayout g_vk_playout;
extern int g_vk_preserve_infnan;
extern int g_vk_preserve_infnan64;
extern int g_vk_flush_denorm;
extern VkShaderStageFlags g_vk_subgroup_stages;
extern VkDescriptorSetLayout g_vk_dslayout[5];  
extern u32 g_vk_stage_sets;  

#define VK_PUSH_STAGES                                                                                                 \
    (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |                                           \
     VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
void vk_pipeline_init(void);

VkSh* vk_shader_get(const GpuChannel* ch, u64 gva, u64 gva_a, int stage, u64 int_mask, u64 sint_mask, u32 rt_int,
                    u32 rt_sint, u32 clip, u64 dc_color, u32 dc_funcs, u32 unfed);
typedef struct {
    VkShaderModule vs, fs;
    VkShaderModule tcs, tes, gs;  
    u32 patch;                    

    u32 topo_class;
    u32 polygon;
    u32 depth_clamp;
    u32 nattr, nbind;
    struct {
        u32 loc, binding, format, offset;
    } attr[32];
    struct {
        u32 divisor;  
    } bind[33];       
    u32 ncolor;
    u32 color[8];
    u32 depth, stencil;
    struct {
        u32 en, sc, dc, cop, sa, da, aop, mask;
    } blend[8];
} PipeKey;
VkPipeline vk_pipeline_get(const PipeKey* k);
VkPipelineCache vk_pipeline_cache(void);  
void vk_pipeline_save(void);
VkShaderModule vk_module_spv(const u32* words, size_t count);
VkShaderModule vk_module_spv_location(const u32* words, size_t count, u32 location);


void vk_dispatch(GpuChannel* ch, const u32* q, u64 code);
void vk_compute_land(void);  

u64 vk_compute_put_aside(void);
void vk_compute_wait(u64 value);                  
void vk_compute_land_through(u64 value);          
void vk_compute_before_write(u64 gva, u64 size);  
void vk_compute_mem_need(u64 a, u64 n);           
void vk_compute_mem_need_map(u64 a, u64 n);       

int vk_compute_gpu_src(u64 gva, u64 len, VkBuffer* buf, u64* off);
extern VkSemaphore g_vk_csem;   
extern u64 g_vk_cs_wait_value;  
extern u64 g_vk_dispatches, g_vk_cs_batches, g_vk_cs_skipped, g_vk_cs_wait_ns;  


void vk_render_end(void);  

void vk_ui_draw(VkCommandBuffer cb, VkImage sc, const int safe[4]);
void vk_ui_swapchain_gone(void);  
void vk_render_clear(GpuChannel* ch, u32 v);
VkViewport vk_render_viewport(const u32* r);
int vk_render_depth_clamp(const u32* r);
VkFrontFace vk_render_front_face(const u32* r);
void vk_render_draw_texture(GpuChannel* ch);
void vk_render_draw(GpuChannel* ch, int indexed, u32 topology, u32 first, u32 count, u32 base_vertex, u32 base_instance,
                    u32 instances);

VkImageViewType vk_tex_view_type(const ShTex* st);
VkImageView vk_tex_bind_view(Surf* t, VkImageViewType want, const u32* tic, const u32* tsc, int num);
VkSampler vk_tex_bind_sampler(const u32* tsc, const ShTex* st, int num, const Surf* t, VkImageViewType want);
extern u64 g_vk_drawn, g_vk_skipped;  

#endif
