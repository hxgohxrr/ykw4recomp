/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_SHADER_H
#define SNX_SHADER_H
#include "common/cpu/aarch64/cpu.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { ST_VS = 0, ST_TCS = 1, ST_TES = 2, ST_GS = 3, ST_FS = 4, ST_CS = 5 };
#define SH_MAXTEX         64
#define SH_INDEXED_TABLES 10
typedef struct {
    u32 cbuf_off;  
    u8 type;       
    u8 array, shadow;
    u8 bindless;
    u8 bl_cbuf;
    u8 bl2;
    u8 bl2_cbuf;  
    u8 flags;     
    u32 bl2_off;  
} ShTex;
static inline int shader_same_indexed_table(const ShTex* a, const ShTex* b) {
    return a->bl_cbuf == b->bl_cbuf && a->cbuf_off == b->cbuf_off && a->flags == b->flags && a->type == b->type &&
           a->array == b->array && a->shadow == b->shadow;
}
typedef struct {
    int stage;
    u32 sph[20];
    u32 cbuf_mask;     
    u32 cbuf_max[18];  
    ShTex tex[SH_MAXTEX];
    int ntex;
    u32 in_mask;        
    u32 attr_int_mask;  
    u32 attr_sint_mask;
    u32 clip_enable;  
    int uses_position_in, uses_clip_mask, writes_layer, writes_point_size;
    int fp32_flush, fp32_preserve;  
    u32 gbuf_cbuf_off[16];
    int ngbuf;        
    int unsupported;  
    char first_unsupported[64];
    u32 cs_local[3];
    int shared_size;
    u32 gbuf_wmask;  
    struct {
        u32 cbuf_off;
        u8 type, bindless, bl_cbuf, kind;  
    } img[8];
    int nimg;  

    u64 tex_int_mask, tex_sint_mask;
    u32 rt_int_mask, rt_sint_mask;
    u32 gp_skip_mask[8];  

    u64 tex_dc_color_mask;
    u32 tex_dc_color_funcs;
    u32 out_mask;  

    u32 in_unfed_mask;
} ShaderInfo;
#define SH_DC_COLOR_MAX  10  


#define SNX_VKB_CB       0   
#define SNX_VKB_SSBO     18  
#define SNX_VKB_TEX      34  
#define SNX_VKB_IMG      (SNX_VKB_TEX + SH_MAXTEX)  
#define SNX_VKB_RS       (SNX_VKB_IMG + 8)          
#define SNX_VKB_FMT      (SNX_VKB_RS + 1)           
#define SNX_VKB_SMAP     (SNX_VKB_FMT + 1)          
#define SNX_VKB_IMGBUF   (SNX_VKB_SMAP + 1)  
#define SNX_VKB_TEXARRAY (SNX_VKB_IMGBUF + 8)    
#define SNX_VKB_TEXFUNCS (SNX_VKB_TEXARRAY + 1)  
#define SNX_VKB_TEXDEPTH (SNX_VKB_TEXFUNCS + 1)  
#define SNX_VKB_TEXMAP   (SNX_VKB_TEXDEPTH + 1)  
#define SNX_VKB_END      (SNX_VKB_TEXMAP + 1)    

#define SNX_TEX_ARRAY    256
#define SNX_TEX_INDEXED  4

extern int g_shader_texture_arrays;

extern int g_shader_stage_point_size;

extern int g_shader_output_layer;

extern int g_shader_geometry;

extern int g_shader_float16;

extern int g_shader_tessellation;

extern int g_shader_clock_scopes;

#define SNX_SH_NOT_LAST (1u << 31)
int shader_vk_set(int stage);

u32* shader_to_spirv(const u8* code, u32 max_bytes, int stage, ShaderInfo* si, size_t* word_count, int can_flush,
                     int can_preserve, int can_preserve_f64, int can_subgroup);
u32* shader_compute_to_spirv(const u8* code, u32 max_bytes, ShaderInfo* si, size_t* word_count, int can_flush,
                             int can_preserve);
extern int (*g_shader_cb_read)(u32 idx, u32 off, u32* out);
int shader_brx_targets(const u64* words, u32 index, u32 count, u32* targets, int max_targets);
u64 shader_hash(const u8* code, u32 max_bytes, u32* out_len);

int shader_corpus_save(const char* dir, const u8* code, u32 len, u32 max_bytes, int stage, const ShaderInfo* si,
                       u64 key);

u8* shader_corpus_load(const char* path, u32* max_bytes, int* stage, ShaderInfo* si);

int shader_merge_dual(const u8* a, const u8* b, u8* out, u32 max_bytes);
#ifdef __cplusplus
}
#endif
#endif
