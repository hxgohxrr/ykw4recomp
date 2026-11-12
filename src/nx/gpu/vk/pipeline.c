/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/runtime/frame_profile.h"
#include "nx/gpu/vk/storage_limits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nx/runtime/main.h"
#include "nx/runtime/options.h"
#include "nx/gpu/gpu.h"
#include "nx/gpu/shader/shader.h"
#include "nx/runtime/platform/log.h"
#include "nx/runtime/platform/plat.h"
#include "nx/gpu/vk/vk.h"
#include "nx/gpu/vk/internal_spv.h"
#if !defined(_WIN32)
#include <unistd.h>
#endif


static char g_cache_dir[1100], g_pcache_path[1200], g_pcache_tmp[1230];
static int g_pcache_dirty;  

VkPipelineLayout g_vk_playout;
VkDescriptorSetLayout g_vk_dslayout[5];
u32 g_vk_stage_sets;
static VkPipelineCache g_pcache;
static PipeKey g_last_pipe_key;
static VkPipeline g_last_pipe;

static const GpuChannel* g_sh_ch;
static int g_sh_bind;

static int sh_cb_read(u32 idx, u32 off, u32* out) {
    if (idx >= 18) return 0;
    u64 a = g_sh_ch->cbind[g_sh_bind][idx];
    u32 sz = g_sh_ch->cbind_size[g_sh_bind][idx];
    u32 limit = sz ? sz : 65536;
    if (!a || limit < 4 || off > limit - 4) return 0;
    gmem_read(a + off, out, 4);
    return 1;
}
static VkCbReader g_rec_read;
static VkCbReads* g_rec;
static int g_rec_failed;
static int rec_read(u32 idx, u32 off, u32* out) {
    int ok = g_rec_read(idx, off, out);
    VkCbReads* reads = g_rec;
    if (reads->n % 16 == 0) {
        VkCbRead* grown = realloc(reads->r, sizeof *grown * (reads->n + 16));
        if (!grown) {
            g_rec_failed = 1;
            return ok;
        }
        reads->r = grown;
    }
    reads->r[reads->n++] = (VkCbRead){idx, off, (u32)ok, ok ? *out : 0};
    return ok;
}
const char* vk_corpus_dir(void) {
    static char dir[sizeof g_exedir + sizeof "captures/shaders"];
    static const char* at = (const char*)-1;
    if (at != (const char*)-1) return at;
    at = getenv("SNX_SHADER_CORPUS");
    if (!at && g_opt_diag) {  
        snprintf(dir, sizeof dir, "%scaptures", g_exedir);
        plat_mkdir(dir);
        snprintf(dir, sizeof dir, "%scaptures/shaders", g_exedir);
        plat_mkdir(dir);
        at = dir;
    }
    return at;
}

const char* vk_spirv_unavailable(const u32* spv, size_t words) {
    for (size_t at = 5; at < words;) {  
        u32 width = spv[at] >> 16;
        if (!width || at + width > words || (spv[at] & 0xffffu) != 17) break;
        if (width == 2 && spv[at + 1] == 10 && !g_vk.feat.shaderFloat64) return "shaderFloat64 is unavailable";
        if (width == 2 && spv[at + 1] == 11 && !g_vk.feat.shaderInt64) return "shaderInt64 is unavailable";
        if (width == 2 && spv[at + 1] == 9 && !g_shader_float16) return "shaderFloat16 is unavailable";
        if (width == 2 && spv[at + 1] == 2 && !g_vk.feat.geometryShader) return "geometryShader is unavailable";
        if (width == 2 && spv[at + 1] == 3 && !g_vk.feat.tessellationShader) return "tessellationShader is unavailable";
        if (width == 2 && spv[at + 1] == 25 && !g_vk.feat.shaderImageGatherExtended)
            return "shaderImageGatherExtended is unavailable";
        if (width == 2 && spv[at + 1] == 32 && !g_vk.feat.shaderClipDistance)
            return "shaderClipDistance is unavailable";
        if (width == 2 && spv[at + 1] == 35 && !g_vk.feat.sampleRateShading) return "sampleRateShading is unavailable";
        if (width == 2 && spv[at + 1] == 4427 && !g_vk.has_shader_draw_parameters)
            return "shaderDrawParameters is unavailable";
        if (width == 2 && spv[at + 1] == 5055 && !g_shader_clock_scopes) return "shaderClock is unavailable";
        at += width;
    }
    return NULL;
}

u32* vk_translate_spirv(const u8* code, u32 max_bytes, int stage, ShaderInfo* si, size_t* words, VkCbReader read,
                        VkCbReads* reads) {
    int (*old)(u32, u32, u32*) = g_shader_cb_read;
    g_rec_read = read;
    g_rec = reads;
    g_rec_failed = 0;
    g_shader_cb_read = rec_read;
    const VkShaderStageFlagBits stages[] = {VK_SHADER_STAGE_VERTEX_BIT,
                                            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                            VK_SHADER_STAGE_GEOMETRY_BIT,
                                            VK_SHADER_STAGE_FRAGMENT_BIT,
                                            VK_SHADER_STAGE_COMPUTE_BIT};
    VkShaderStageFlagBits shader_stage = stages[stage];
    u32* result = shader_to_spirv(code, max_bytes, stage, si, words, g_vk_flush_denorm, g_vk_preserve_infnan,
                                  g_vk_preserve_infnan64, (g_vk_subgroup_stages & shader_stage) != 0);
    g_shader_cb_read = old;
    g_rec = NULL;
    if (g_rec_failed) {
        free(result);
        return NULL;
    }
    if (result) {
        const char* reason = vk_spirv_unavailable(result, *words);

        for (int i = 0; i < si->ntex && !reason; i++)
            if (si->tex[i].flags & 2) reason = "multisample texture binding unavailable";
        if (reason) {
            snprintf(si->first_unsupported, sizeof si->first_unsupported, "%s", reason);
            si->unsupported = 1;
            free(result);
            return NULL;
        }
    }
    return result;
}


u64 vk_shader_hash(const u8* code, u32 max_bytes, u32* out_len, VkCbReader read) {
    int (*old)(u32, u32, u32*) = g_shader_cb_read;
    g_shader_cb_read = read;
    u64 h = shader_hash(code, max_bytes, out_len);
    g_shader_cb_read = old;
    return h;
}
int vk_cb_reads_hold(const VkCbReads* reads, VkCbReader read) {
    for (u32 i = 0; i < reads->n; i++) {
        const VkCbRead* c = &reads->r[i];
        u32 v = 0;
        int ok = read(c->idx, c->off, &v);
        if ((u32)ok != c->ok || (ok && v != c->v)) return 0;
    }
    return 1;
}

u32 g_vk_buffer_image_slots;


static VkDescriptorType binding_type(u32 i) {
    if (i == SNX_VKB_SMAP) return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    if (i == SNX_VKB_TEXARRAY || i == SNX_VKB_TEXDEPTH) return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    if (i == SNX_VKB_TEXFUNCS || i == SNX_VKB_TEXMAP) return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    if (i >= SNX_VKB_IMGBUF) return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    return i < SNX_VKB_SSBO  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
           : i < SNX_VKB_TEX ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
           : i < SNX_VKB_IMG ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
           : i < SNX_VKB_RS  ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                             : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}
void vk_pool_sizes(VkDescriptorPoolSize* sz, u32 sets) {
    static const VkDescriptorType ty[VK_POOL_TYPES] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER};
    int k;
    for (k = 0; k < VK_POOL_TYPES; k++) {
        u32 n = 0;
        u32 i;
        for (i = 0; i < SNX_VKB_END; i++)
            if (binding_type(i) == ty[k]) n += (i == SNX_VKB_TEXARRAY || i == SNX_VKB_TEXDEPTH) ? SNX_TEX_ARRAY : 1;
        sz[k] = (VkDescriptorPoolSize){ty[k], n * sets};
    }
}

void vk_pipeline_init(void) {
    g_last_pipe = VK_NULL_HANDLE;

    static const VkShaderStageFlags vis[5] = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                                              VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                              VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                              VK_SHADER_STAGE_GEOMETRY_BIT};

    g_vk_stage_sets = g_vk.props.limits.maxBoundDescriptorSets >= 5 ? 5 : 2;
    for (int s = 0; s < 5; s++) {
        VkDescriptorSetLayoutBinding b[SNX_VKB_END];
        u32 count = 0;
        u32 image_slots = g_vk.props.limits.maxPerStageDescriptorStorageImages;
        u32 buffer_slots;
        u32 i;
        if (image_slots > g_vk.props.limits.maxDescriptorSetStorageImages)
            image_slots = g_vk.props.limits.maxDescriptorSetStorageImages;

        buffer_slots = image_slots;
        if (image_slots > SNX_VKB_RS - SNX_VKB_IMG) image_slots = SNX_VKB_RS - SNX_VKB_IMG;
        buffer_slots -= image_slots;
        if (buffer_slots > SNX_VKB_TEXARRAY - SNX_VKB_IMGBUF) buffer_slots = SNX_VKB_TEXARRAY - SNX_VKB_IMGBUF;
        g_vk_buffer_image_slots = buffer_slots;

        if (g_vk.props.limits.maxPerStageDescriptorSamplers < SH_MAXTEX + 2 * SNX_TEX_ARRAY ||
            g_vk.props.limits.maxPerStageDescriptorSampledImages < SH_MAXTEX + 2 * SNX_TEX_ARRAY ||
            g_vk.props.limits.maxDescriptorSetSamplers < 5 * (SH_MAXTEX + 2 * SNX_TEX_ARRAY) ||
            g_vk.props.limits.maxDescriptorSetSampledImages < 5 * (SH_MAXTEX + 2 * SNX_TEX_ARRAY))
            g_shader_texture_arrays = 0;
        for (i = 0; i < SNX_VKB_END; i++) {
            int image = i >= SNX_VKB_IMG && i < SNX_VKB_RS;
            if (image && (s >= 2 || i - SNX_VKB_IMG >= image_slots)) continue;  
            if (i >= SNX_VKB_TEXARRAY && !g_shader_texture_arrays) continue;
            if (i >= SNX_VKB_IMGBUF && i < SNX_VKB_TEXARRAY && (s != 0 || i - SNX_VKB_IMGBUF >= buffer_slots)) continue;
            if (i >= SNX_VKB_IMGBUF && i < SNX_VKB_TEXARRAY) image = 1;
            b[count++] = (VkDescriptorSetLayoutBinding){
                .binding = i,
                .descriptorType = binding_type(i),
                .descriptorCount = (i == SNX_VKB_TEXARRAY || i == SNX_VKB_TEXDEPTH) ? SNX_TEX_ARRAY : 1,
                .stageFlags = image ? (s == 0 ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT) : vis[s]};
        }
        VkDescriptorSetLayoutCreateInfo dl = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = count, .pBindings = b};
        if (!VK_OK(vkCreateDescriptorSetLayout(g_vk.dev, &dl, NULL, &g_vk_dslayout[s])))
            fatal("vulkan: cannot create shader descriptor layout");
    }
    {
        VkDescriptorPoolSize n[VK_POOL_TYPES];
        vk_pool_sizes(n, 1);
        const VkPhysicalDeviceLimits* l = &g_vk.props.limits;
        if (n[0].descriptorCount > l->maxPerStageDescriptorUniformBuffers ||
            n[1].descriptorCount > l->maxPerStageDescriptorStorageBuffers ||
            n[2].descriptorCount > l->maxPerStageDescriptorSamplers ||
            n[2].descriptorCount > l->maxPerStageDescriptorSampledImages ||
            n[3].descriptorCount > l->maxPerStageDescriptorStorageImages ||
            n[0].descriptorCount + n[1].descriptorCount + n[2].descriptorCount + n[3].descriptorCount + 8 >
                l->maxPerStageResources)
            WARN("vulkan: a stage's descriptor set (%u uniform and %u storage buffers, %u textures, %u images) is "
                 "past this GPU's per-stage limits (%u, %u, %u, %u): pipelines may fail\n",
                 n[0].descriptorCount, n[1].descriptorCount, n[2].descriptorCount, n[3].descriptorCount,
                 l->maxPerStageDescriptorUniformBuffers, l->maxPerStageDescriptorStorageBuffers,
                 l->maxPerStageDescriptorSamplers, l->maxPerStageDescriptorStorageImages);
    }

    VkPushConstantRange pc = {VK_PUSH_STAGES, 0, 32};
    VkPipelineLayoutCreateInfo pl = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                     .setLayoutCount = g_vk_stage_sets,
                                     .pSetLayouts = g_vk_dslayout,
                                     .pushConstantRangeCount = 1,
                                     .pPushConstantRanges = &pc};
    if (!VK_OK(vkCreatePipelineLayout(g_vk.dev, &pl, NULL, &g_vk_playout)))
        fatal("vulkan: cannot create shader pipeline layout");
    snprintf(g_cache_dir, sizeof g_cache_dir, "%sshader_cache", g_cachedir);
    plat_mkdir(g_cache_dir);
    snprintf(g_pcache_path, sizeof g_pcache_path, "%s/vk_pipelines.bin", g_cache_dir);

#if defined(_WIN32)
    unsigned pid = (unsigned)GetCurrentProcessId();
#else
    unsigned pid = (unsigned)getpid();
#endif
    snprintf(g_pcache_tmp, sizeof g_pcache_tmp, "%s.%u.tmp", g_pcache_path, pid);

    VkPipelineCacheCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    PFileMap data;
    if (!plat_file_map(&data, g_pcache_path) && data.data) {
        ci.initialDataSize = (size_t)data.size;
        ci.pInitialData = data.data;
    }
    if (!VK_OK(vkCreatePipelineCache(g_vk.dev, &ci, NULL, &g_pcache)) && ci.initialDataSize) {
        ci.initialDataSize = 0;
        ci.pInitialData = NULL;
        VK_OK(vkCreatePipelineCache(g_vk.dev, &ci, NULL, &g_pcache));
    }
    if (ci.initialDataSize) LOG("vulkan: pipeline cache loaded (%zu KB)\n", ci.initialDataSize >> 10);
    plat_file_unmap(&data);  
}



static VkSh* g_shh[1024];

typedef struct {
    u64 gva, gva_a, gw;
    u32 mask[20], epoch, wseen;
    int stage;
    VkSh* s;
} ShA;
static ShA g_sha[4096];

static int code_hash(u64 gva, u32 len, u64* hash) {
    u8* p = gmem_ptr(gva, len);
    if (p) {
        *hash = vk_hash(p, len);
        return 1;
    }
    u8* t = malloc(len ? len : 1);
    if (!t) return 0;
    gmem_read(gva, t, len);
    *hash = vk_hash(t, len);
    free(t);
    return 1;
}
static int code_same(const ShA* ca, u64 gva, u32 len, u64 qh) {
    if (!gpu_mem_dirty_since(ca->gw, gva, len) && vk_written_since(gmem_ptr(gva, len), len, ca->wseen) == 0) return 1;
    u64 hash;
    return code_hash(gva, len, &hash) && hash == qh;
}

VkSh* vk_shader_get(const GpuChannel* ch, u64 gva, u64 gva_a, int stage, u64 int_mask, u64 sint_mask, u32 rt_int,
                    u32 rt_sint, u32 clip, u64 dc_color, u32 dc_funcs, u32 unfed) {
    g_sh_ch = ch;
    g_sh_bind = stage <= ST_FS ? stage : 0;
    u32 mask[20] = {(u32)int_mask,
                    (u32)(int_mask >> 32),
                    (u32)sint_mask,
                    (u32)(sint_mask >> 32),
                    rt_int,
                    rt_sint,
                    clip,
                    (u32)dc_color,
                    (u32)(dc_color >> 32),
                    dc_funcs,
                    unfed};
    if (stage == ST_GS && ch) memcpy(mask + 12, ch->r3d + 0x490, 8 * sizeof(u32));
    u32 mix = (u32)vk_hash(mask, 12 * sizeof(u32));
    if (stage == ST_GS) mix ^= (u32)vk_hash(mask + 12, 8 * sizeof(u32));
    ShA* set = &g_sha[(u32)((gva >> 6) ^ (gva >> 18) ^ (gva_a >> 4) ^ mix ^ ((u32)stage << 11)) & 4094];
    ShA* ca = NULL;
    for (int w = 0; w < 2 && !ca; w++)
        if (set[w].s && set[w].gva == gva && set[w].gva_a == gva_a && set[w].stage == stage &&
            !memcmp(set[w].mask, mask, sizeof mask))
            ca = &set[w];
    if (ca) {
        VkSh* s = ca->s;
        int same = ca->epoch == g_vk_epoch ||
                   (code_same(ca, gva, s->clen, s->qh) && (!gva_a || code_same(ca, gva_a, s->clen_a, s->qh_a)));
        if (same && ca->epoch != g_vk_epoch) {
            ca->epoch = g_vk_epoch;
            ca->wseen = vk_ww_now();
            ca->gw = gpu_mem_generation();
        }
        if (same && vk_cb_reads_hold(&s->cbr, sh_cb_read)) return s;
    } else  
        ca = !set[0].s || (set[1].s && (i32)(set[0].epoch - set[1].epoch) <= 0) ? &set[0] : &set[1];
    u32 wseen = vk_ww_now();
    u64 gw = gpu_mem_generation();
    u8* code = malloc(0x10000);
    if (!code) return NULL;
    gmem_read_code(gva, code, 0x10000);
    u32 len, len_a = 0;
    u64 qh, qh_a = 0;
    vk_shader_hash(code, 0x10000, &len, sh_cb_read);
    qh = vk_hash(code, len);
    if (gva_a) {  
        u8* a = malloc(0x10000);
        if (!a) {
            free(code);
            return NULL;
        }
        gmem_read_code(gva_a, a, 0x10000);
        vk_shader_hash(a, 0x10000, &len_a, sh_cb_read);
        qh_a = vk_hash(a, len_a);
        u8* m = malloc(0x10000);
        if (!m) {
            free(a);
            free(code);
            return NULL;
        }

        int (*old)(u32, u32, u32*) = g_shader_cb_read;
        g_shader_cb_read = sh_cb_read;
        int merged = shader_merge_dual(a, code, m, 0x10000);
        g_shader_cb_read = old;
        if (merged) {
            free(code);
            code = m;
        } else {
            free(m);
            WARN_LIMIT(8, "vulkan: VertexA %llx and VertexB %llx couldn't be merged: VertexB runs alone\n",
                       (unsigned long long)gva_a, (unsigned long long)gva);
        }
        free(a);
    }
    u32 mlen;
    u64 h = vk_shader_hash(code, 0x10000, &mlen, sh_cb_read);
    VkSh* s = NULL;
    for (VkSh* q = g_shh[h & 1023]; q && !s; q = q->next)
        if (q->hash == h && q->stage == stage && !memcmp(q->mask, mask, sizeof mask) &&
            vk_cb_reads_hold(&q->cbr, sh_cb_read))
            s = q;
    if (!s) {
        s = calloc(1, sizeof *s);
        if (!s) {
            free(code);
            return NULL;
        }
        s->hash = h;
        s->stage = stage;
        memcpy(s->mask, mask, sizeof mask);
        s->clen = len;
        s->qh = qh;
        s->clen_a = len_a;
        s->qh_a = qh_a;
        if (stage == ST_FS) {
            s->si.tex_int_mask = int_mask;
            s->si.tex_sint_mask = sint_mask;
            s->si.rt_int_mask = rt_int;
            s->si.rt_sint_mask = rt_sint;
            s->si.tex_dc_color_mask = dc_color;
            s->si.tex_dc_color_funcs = dc_funcs;
            s->si.in_unfed_mask = unfed;
        } else {
            s->si.attr_int_mask = int_mask;
            s->si.attr_sint_mask = sint_mask;
            s->si.rt_int_mask = rt_int;  
            s->si.clip_enable = clip;
            memcpy(s->si.gp_skip_mask, mask + 12, sizeof s->si.gp_skip_mask);
        }
        if (vk_corpus_dir()) shader_corpus_save(vk_corpus_dir(), code, mlen, 0x10000, stage, &s->si, h ^ mix);
        size_t nw = 0;
        u32* spv = vk_translate_spirv(code, 0x10000, stage, &s->si, &nw, sh_cb_read, &s->cbr);
        if (spv && s->si.nimg) {
            int needs_unformatted = 0;
            for (int i = 0; i < s->si.nimg; i++) needs_unformatted |= s->si.img[i].kind == 0;
            if (stage != ST_FS || !vk_storage_images_supported(s->si.nimg,
                    g_vk.props.limits.maxPerStageDescriptorStorageImages,
                    g_vk.props.limits.maxDescriptorSetStorageImages) ||
                (needs_unformatted && !g_vk.feat.shaderStorageImageWriteWithoutFormat)) {
                free(spv);
                spv = NULL;
                snprintf(s->si.first_unsupported, sizeof s->si.first_unsupported,
                         "fragment storage image feature or descriptor limit");
            }
        }
        if (spv) {
            VkShaderModuleCreateInfo mi = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = nw * 4, .pCode = spv};
            if (!VK_OK(vkCreateShaderModule(g_vk.dev, &mi, NULL, &s->mod))) s->failed = 1;
            free(spv);
        } else {
            s->failed = 1;
            WARN("vulkan: shader %016llx (stage %d) not translated: %s\n", (unsigned long long)h, stage,
                 s->si.first_unsupported[0] ? s->si.first_unsupported : "unsupported feature or float controls");
        }
        s->next = g_shh[h & 1023];
        g_shh[h & 1023] = s;
    }
    free(code);
    ca->gva = gva;
    ca->gva_a = gva_a;
    memcpy(ca->mask, mask, sizeof mask);
    ca->stage = stage;
    ca->epoch = g_vk_epoch;
    ca->wseen = wseen;
    ca->gw = gw;
    ca->s = s;
    return s;
}

VkShaderModule vk_module_spv(const u32* spv, size_t nw) {
    VkShaderModuleCreateInfo mi = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = nw * 4, .pCode = spv};
    VkShaderModule m = VK_NULL_HANDLE;
    VK_OK(vkCreateShaderModule(g_vk.dev, &mi, NULL, &m));
    return m;
}
VkShaderModule vk_module_spv_location(const u32* source, size_t nw, u32 location) {
    u32* spv = malloc(nw * sizeof *spv);
    if (!spv) return VK_NULL_HANDLE;
    memcpy(spv, source, nw * sizeof *spv);
    int found = 0;
    for (size_t i = 5; i < nw;) {
        u32 count = spv[i] >> 16, op = spv[i] & 0xffffu;
        if (!count || i + count > nw) break;
        if (op == 71 && count == 4 && spv[i + 2] == 30) {
            spv[i + 3] = location;
            found++;
        }
        i += count;
    }
    VkShaderModule result = found == 1 ? vk_module_spv(spv, nw) : VK_NULL_HANDLE;
    free(spv);
    return result;
}

static VkShaderModule null_fs(void) {
    static VkShaderModule m;
    if (!m) m = vk_module_spv(snx_null_fs_spv, sizeof snx_null_fs_spv / sizeof *snx_null_fs_spv);
    return m;
}


typedef struct Pipe {
    PipeKey k;
    u64 h;
    VkPipeline p;
    struct Pipe* next;
} Pipe;
#define PIPEH 4096
static Pipe* g_pipeh[PIPEH];

static VkPipeline pipe_create(const PipeKey* k) {
    VkPipelineShaderStageCreateInfo st[5];
    VkShaderModule modules[5];
    static const VkShaderStageFlagBits bits[5] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                                  VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
    VkPipelineTessellationStateCreateInfo ts;
    u32 nst = 0, s;
    modules[0] = k->vs;
    modules[1] = k->tcs;
    modules[2] = k->tes;
    modules[3] = k->gs;
    modules[4] = k->fs ? k->fs : null_fs();
    for (s = 0; s < 5; s++) {
        if (!modules[s]) continue;
        memset(&st[nst], 0, sizeof st[nst]);
        st[nst].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[nst].stage = bits[s];
        st[nst].module = modules[s];
        st[nst].pName = "main";
        nst++;
    }
    memset(&ts, 0, sizeof ts);
    ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ts.patchControlPoints = k->patch;
    VkVertexInputBindingDescription vb[33];
    VkVertexInputBindingDivisorDescriptionEXT dv[33];
    u32 ndv = 0;
    for (u32 i = 0; i < k->nbind; i++) {
        vb[i] = (VkVertexInputBindingDescription){.binding = i,
                                                  .stride = 0,  
                                                  .inputRate = k->bind[i].divisor ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                                                  : VK_VERTEX_INPUT_RATE_VERTEX};
        if (k->bind[i].divisor > 1 && g_vk.has_divisor)
            dv[ndv++] = (VkVertexInputBindingDivisorDescriptionEXT){i, k->bind[i].divisor};
    }
    VkVertexInputAttributeDescription va[32];
    for (u32 i = 0; i < k->nattr; i++)
        va[i] = (VkVertexInputAttributeDescription){k->attr[i].loc, k->attr[i].binding, (VkFormat)k->attr[i].format,
                                                    k->attr[i].offset};
    VkPipelineVertexInputDivisorStateCreateInfoEXT dvi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,
        .vertexBindingDivisorCount = ndv,
        .pVertexBindingDivisors = dv};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                               .pNext = ndv ? &dvi : NULL,
                                               .vertexBindingDescriptionCount = k->nbind,
                                               .pVertexBindingDescriptions = vb,
                                               .vertexAttributeDescriptionCount = k->nattr,
                                               .pVertexAttributeDescriptions = va};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = (VkPrimitiveTopology)k->topo_class};
    VkPipelineViewportStateCreateInfo vp = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .depthClampEnable = k->depth_clamp,
                                                 .polygonMode = (VkPolygonMode)k->polygon,
                                                 .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState cba[8];
    int changed = 0;
    for (u32 i = 0; i < k->ncolor; i++) {
        cba[i] = (VkPipelineColorBlendAttachmentState){.blendEnable = k->blend[i].en,
                                                       .srcColorBlendFactor = (VkBlendFactor)k->blend[i].sc,
                                                       .dstColorBlendFactor = (VkBlendFactor)k->blend[i].dc,
                                                       .colorBlendOp = (VkBlendOp)k->blend[i].cop,
                                                       .srcAlphaBlendFactor = (VkBlendFactor)k->blend[i].sa,
                                                       .dstAlphaBlendFactor = (VkBlendFactor)k->blend[i].da,
                                                       .alphaBlendOp = (VkBlendOp)k->blend[i].aop,

                                                       .colorWriteMask = k->fs ? k->blend[i].mask : 0};
        if (!g_vk.feat.dualSrcBlend) {  
            VkBlendFactor* f[4] = {&cba[i].srcColorBlendFactor, &cba[i].dstColorBlendFactor,
                                   &cba[i].srcAlphaBlendFactor, &cba[i].dstAlphaBlendFactor};
            for (int j = 0; j < 4; j++) {
                VkBlendFactor o = *f[j];
                *f[j] = o == VK_BLEND_FACTOR_SRC1_COLOR             ? VK_BLEND_FACTOR_SRC_COLOR
                        : o == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR ? VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
                        : o == VK_BLEND_FACTOR_SRC1_ALPHA           ? VK_BLEND_FACTOR_SRC_ALPHA
                        : o == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                                                                    : o;
                changed |= *f[j] != o;
            }
        }
    }
    if (!g_vk.feat.independentBlend)  
        for (u32 i = 1; i < k->ncolor; i++) {
            changed |= memcmp(&cba[i], &cba[0], sizeof cba[0]) != 0;
            cba[i] = cba[0];
        }
    if (changed) {
        LOG_LIMIT(
            1, "vulkan: blending that needs dual-source or independent blend, which this GPU lacks, is simplified\n");
    }
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                              .attachmentCount = k->ncolor,
                                              .pAttachments = cba};
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
                            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
                            VK_DYNAMIC_STATE_CULL_MODE,
                            VK_DYNAMIC_STATE_FRONT_FACE,
                            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                            VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
                            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
                            VK_DYNAMIC_STATE_STENCIL_OP,
                            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
                            VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
                            VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
                            VK_DYNAMIC_STATE_LINE_WIDTH,
                            VK_DYNAMIC_STATE_DEPTH_BIAS,
                            VK_DYNAMIC_STATE_BLEND_CONSTANTS,
                            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
                            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                            VK_DYNAMIC_STATE_DEPTH_BOUNDS,
                            VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};

    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                           .dynamicStateCount = sizeof dyn / sizeof dyn[0] - (k->nbind ? 0 : 1),
                                           .pDynamicStates = dyn};
    VkFormat cf[8];
    for (u32 i = 0; i < k->ncolor; i++) cf[i] = (VkFormat)k->color[i];
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                        .colorAttachmentCount = k->ncolor,
                                        .pColorAttachmentFormats = cf,
                                        .depthAttachmentFormat = (VkFormat)k->depth,
                                        .stencilAttachmentFormat = (VkFormat)k->stencil};
    VkGraphicsPipelineCreateInfo gi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                       .pNext = &ri,
                                       .stageCount = nst,
                                       .pStages = st,
                                       .pVertexInputState = &vi,
                                       .pInputAssemblyState = &ia,
                                       .pTessellationState = k->tes ? &ts : NULL,
                                       .pViewportState = &vp,
                                       .pRasterizationState = &rs,
                                       .pMultisampleState = &ms,
                                       .pDepthStencilState = &ds,
                                       .pColorBlendState = &cb,
                                       .pDynamicState = &dy,
                                       .layout = g_vk_playout};
    VkPipeline p = VK_NULL_HANDLE;
    u64 compile_start = plat_ns();
    VK_OK(vkCreateGraphicsPipelines(g_vk.dev, g_pcache, 1, &gi, NULL, &p));
    u64 compile_ns = plat_ns() - compile_start;
    frame_profile_add(FP_PIPELINE_GRAPHICS, compile_ns, 1);
    if (compile_ns >= 1000000000ull)
        WARN("vulkan: graphics pipeline compilation took %llu ms\n", (unsigned long long)(compile_ns / 1000000));
    return p;
}

VkPipeline vk_pipeline_get(const PipeKey* k) {

    static int memo = -1;
    if (memo < 0) {
        const char* value = getenv("SNX_VK_PIPELINE_MEMO");
        // Pipelines and their complete keys are immutable for the device lifetime.
        // Keep SNX_VK_PIPELINE_MEMO=0 for diagnostic comparisons.
        memo = !value || strcmp(value, "0") != 0;
    }
    if (memo && g_last_pipe && !memcmp(k, &g_last_pipe_key, sizeof *k)) return g_last_pipe;
    u64 h = vk_hash(k, sizeof *k);
    u32 b = (u32)(h ^ (h >> 32)) & (PIPEH - 1);
    for (Pipe* p = g_pipeh[b]; p; p = p->next)
        if (p->h == h && !memcmp(&p->k, k, sizeof *k)) {
            if (memo) { g_last_pipe_key = *k; g_last_pipe = p->p; }
            return p->p;
        }
    Pipe* p = calloc(1, sizeof *p);
    p->k = *k;
    p->h = h;
    p->p = pipe_create(k);
    __atomic_store_n(&g_pcache_dirty, 1, __ATOMIC_RELAXED);
    p->next = g_pipeh[b];
    g_pipeh[b] = p;
    if (memo) { g_last_pipe_key = *k; g_last_pipe = p->p; }
    return p->p;
}

VkPipelineCache vk_pipeline_cache(void) {
    __atomic_store_n(&g_pcache_dirty, 1, __ATOMIC_RELAXED);
    return g_pcache;
}

static PMutex g_psave = PMUTEX_INIT;
void vk_pipeline_save(void) {
    pm_lock(&g_psave);
    if (!g_pcache || !__atomic_exchange_n(&g_pcache_dirty, 0, __ATOMIC_RELAXED)) {
        pm_unlock(&g_psave);
        return;
    }
    size_t n = 0;
    void* d = NULL;
    if (vkGetPipelineCacheData(g_vk.dev, g_pcache, &n, NULL) == VK_SUCCESS && n) d = malloc(n);
    if (d && vkGetPipelineCacheData(g_vk.dev, g_pcache, &n, d) == VK_SUCCESS) {
        FILE* f = fopen(g_pcache_tmp, "wb");
        if (f) {
            int ok = fwrite(d, 1, n, f) == n;
            if (fclose(f)) ok = 0;
            if (ok)
                plat_replace_file(g_pcache_tmp, g_pcache_path);  
            else
                remove(g_pcache_tmp);
        }
    }
    free(d);
    pm_unlock(&g_psave);
}
