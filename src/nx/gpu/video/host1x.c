/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/gpu/video/host1x_int.h"



typedef struct {
    u32 iova, size;
    u64 addr;
    u32 h;
} IoMap;
#define IOVA_BASE  0x10000000ull
#define IOVA_END   0x100000000ull  
#define IOVA_GUARD 0x10000ull
static IoMap g_iom[1024];
static int g_niom;

static PMutex g_iomlk = PMUTEX_INIT;
static PMutex g_hlk = PMUTEX_INIT;
static u64 iova_span(u32 size) { return (((u64)size + 0xFFFFu) & ~0xFFFFull) + IOVA_GUARD; }

static u32 iova_map(u32 h, u32* err) {
    u64 a;
    u32 sz;

    pm_lock(&g_iomlk);
    if (!nvmap_get(h, &a, &sz) || !a || !sz) {  
        pm_unlock(&g_iomlk);
        WARN_LIMIT(4, "video: nvmap handle %u has no memory or no size, not mapped for NVDEC/VIC\n", h);
        *err = 4;
        return 0;
    }
    for (int i = 0; i < g_niom; i++)
        if (g_iom[i].h == h && g_iom[i].addr == a) {
            u32 io = g_iom[i].iova;
            pm_unlock(&g_iomlk);
            return io;
        }
    u64 need = iova_span(sz), base = IOVA_BASE;
    int at = 0;
    for (; at < g_niom; at++) {
        if (base + need <= g_iom[at].iova) break;
        u64 e = g_iom[at].iova + iova_span(g_iom[at].size);
        if (e > base) base = e;
    }
    if (g_niom >= 1024 || base + need > IOVA_END) {
        int nm = g_niom;
        pm_unlock(&g_iomlk);
        WARN_LIMIT(8,
                   "video: no room to map nvmap handle %u (%u bytes, %d mappings): NVDEC and VIC see unmapped memory\n",
                   h, sz, nm);
        *err = 6;
        return 0;
    }
    memmove(&g_iom[at + 1], &g_iom[at], (size_t)(g_niom - at) * sizeof g_iom[0]);
    g_iom[at].iova = (u32)base;
    g_iom[at].size = sz;
    g_iom[at].addr = a;
    g_iom[at].h = h;
    g_niom++;
    pm_unlock(&g_iomlk);
    return (u32)base;
}
void host1x_forget_handle(u32 h) {
    pm_lock(&g_iomlk);
    int k = 0;
    for (int i = 0; i < g_niom; i++)
        if (g_iom[i].h != h) g_iom[k++] = g_iom[i];
    g_niom = k;
    pm_unlock(&g_iomlk);
}

u8* iova_ptr(u64 iova, u64 len) {
    u8* p = NULL;
    pm_lock(&g_iomlk);
    for (int i = 0; i < g_niom && !p; i++) {
        IoMap* m = &g_iom[i];
        if (len && len <= m->size && iova >= m->iova && iova - m->iova <= m->size - len)
            p = GP(m->addr + (iova - m->iova));
    }
    pm_unlock(&g_iomlk);
    return p;
}



#define H1_NSYNCPT 16  
struct H1Ch {
    int is_vic;
    u32 syncpt[H1_NSYNCPT];  
    u32 regs[0x1000 / 4];
    u32 method;
    NvdecCtx* dec;  
};
static void dev_method(H1Ch* ch, u32 m, u32 v) {
    if (m >= 0x1000 / 4) return;  
    u32 off = m << 2;
    ch->regs[off / 4] = v;
    if (off == 0x300) {
        if (ch->is_vic)
            vic_execute(ch->regs);
        else
            nvdec_execute(ch->dec, ch->regs);
    }
}
static void reg_write(H1Ch* ch, u32 cls, u32 off, u32 v) {

    if (cls == 0x1) return;
    if (off == 0x10)
        ch->method = v;
    else if (off == 0x11)
        dev_method(ch, ch->method, v);
}
static void run_words(H1Ch* ch, const u32* w, u32 n) {
    u32 cls = ch->is_vic ? 0x5D : 0xF0;
    for (u32 i = 0; i < n;) {
        u32 x = w[i++], op = x >> 28;
        switch (op) {
            case 0: {
                cls = (x >> 6) & 0x3FF;
                u32 off = (x >> 16) & 0xFFF, mask = x & 0x3F;
                for (int b = 0; b < 6; b++)
                    if (mask & (1u << b)) {
                        if (i >= n) return;
                        reg_write(ch, cls, off + b, w[i++]);
                    }
                break;
            }
            case 1: {
                u32 off = (x >> 16) & 0xFFF, c = x & 0xFFFF;
                for (u32 k = 0; k < c && i < n; k++) reg_write(ch, cls, off + k, w[i++]);
                break;
            }
            case 2: {
                u32 off = (x >> 16) & 0xFFF, c = x & 0xFFFF;
                for (u32 k = 0; k < c && i < n; k++) reg_write(ch, cls, off, w[i++]);
                break;
            }
            case 3: {
                u32 off = (x >> 16) & 0xFFF, mask = x & 0xFFFF;
                for (int b = 0; b < 16; b++)
                    if (mask & (1u << b)) {
                        if (i >= n) return;
                        reg_write(ch, cls, off + b, w[i++]);
                    }
                break;
            }
            case 4: reg_write(ch, cls, (x >> 16) & 0xFFF, x & 0xFFFF); break;
            default: {
                LOG_LIMIT(6, "video: host1x opcode %u not handled (word %08x)\n", op, x);
                return;
            }
        }
    }
}


H1Ch* host1x_open(int is_vic) {
    H1Ch* ch = calloc(1, sizeof *ch);
    if (!ch) return NULL;
    ch->is_vic = is_vic;
    if (!is_vic && !(ch->dec = nvdec_new())) {
        free(ch);
        return NULL;
    }
    return ch;
}
void host1x_close(H1Ch* ch) {
    if (!ch) return;
    pm_lock(&g_hlk);
    for (int i = 0; i < H1_NSYNCPT; i++)
        if (ch->syncpt[i]) syncpt_free(ch->syncpt[i]);
    nvdec_free(ch->dec);
    pm_unlock(&g_hlk);
    free(ch);
}

static u32 ch_syncpt(H1Ch* ch, u32 i) {
    if (!ch->syncpt[i]) ch->syncpt[i] = syncpt_alloc();
    return ch->syncpt[i];
}

static int ch_owns(const H1Ch* ch, u32 id) {
    for (int i = 0; id && i < H1_NSYNCPT; i++)
        if (ch->syncpt[i] == id) return 1;
    return 0;
}
u32 host1x_ioctl(H1Ch* ch, u32 cmd, u8* d, u32 n) {
    int is_vic = ch->is_vic;
    pm_lock(&g_hlk);
    u32 r = 0, grp = (cmd >> 8) & 0xFF, nr = cmd & 0xFF;
    if (grp == 0x48) {
        if (nr != 0x01)  
            LOG_LIMIT(8, "video: %s ioctl %08x not handled\n", is_vic ? "vic" : "nvdec", cmd);
    } else if (grp == 0)
        switch (nr) {
            case 0x01: {  
                u32 ncb = rd32(d, 0), nrel = rd32(d, 4), nsp = rd32(d, 8), nfe = rd32(d, 12);

                u64 cbo = 16, spo = cbo + (u64)ncb * 12 + (u64)nrel * 16 + (u64)nrel * 4, feo = spo + (u64)nsp * 20;
                if (feo > n) {
                    r = 4;  
                    break;
                }

                u32 k = 0;
                while (k < nsp && ch_owns(ch, rd32(d, (u32)(spo + (u64)k * 20)))) k++;
                if (k < nsp) {
                    WARN_LIMIT(4, "video: %s submit increments syncpoint %u, not one of its channel's, refused\n",
                               is_vic ? "vic" : "nvdec", rd32(d, (u32)(spo + (u64)k * 20)));
                    r = 4;
                    break;
                }
                if (nrel)
                    LOG_LIMIT(1, "video: %s submit has %u relocations, which aren't applied\n",
                              is_vic ? "vic" : "nvdec", nrel);
                for (u32 i = 0; i < ncb; i++) {
                    u32 cb = (u32)(cbo + (u64)i * 12);
                    u32 mem = rd32(d, cb), off = rd32(d, cb + 4), wc = rd32(d, cb + 8);
                    u64 a;
                    u32 msz;
                    nvmap_get(mem, &a, &msz);

                    if (!a || !wc || wc > 0x100000 || (off & 3) || (u64)off + (u64)wc * 4 > msz) {

                        LOG_LIMIT(4,
                                  "video: %s command buffer skipped (nvmap %u, offset %x, %u words, buffer %u bytes)\n",
                                  is_vic ? "vic" : "nvdec", mem, off, wc, msz);
                        continue;
                    }
                    run_words(ch, (const u32*)GP(a + off), wc);
                }
                for (u32 i = 0; i < nsp; i++) {
                    u32 sp = (u32)(spo + (u64)i * 20);
                    u32 id = rd32(d, sp), inc = rd32(d, sp + 4);
                    syncpt_add(id, inc);
                    if (i < nfe && feo + (u64)i * 4 + 4 <= n) {
                        u32 v = syncpt_read(id);
                        memcpy(d + feo + (u64)i * 4, &v, 4);
                    }
                }
                LOG_LIMIT(
                    3,
                    "video: %s submit: %u command buffers, %u relocs, %u syncpoint increments, %u fences (%u bytes)\n",
                    is_vic ? "vic" : "nvdec", ncb, nrel, nsp, nfe, n);
                break;
            }
            case 0x02: {  
                u32 i = rd32(d, 0);
                if (i >= H1_NSYNCPT) {
                    r = 4;  
                    break;
                }
                u32 v = ch_syncpt(ch, i);
                memcpy(d + 4, &v, 4);
                break;
            }
            case 0x03: {
                u32 v = 0;
                memcpy(d + 4, &v, 4);
                break;
            }  
            case 0x07: break;  
            case 0x09: {       
                u32 ne = rd32(d, 0);
                for (u32 i = 0; i < ne && 12 + (u64)i * 8 + 8 <= n; i++) {
                    u32 h = rd32(d, 12 + i * 8), err = 0;
                    u32 io = iova_map(h, &err);
                    memcpy(d + 12 + i * 8 + 4, &io, 4);
                    if (err && !r) r = err;
                }
                break;
            }

            case 0x0A: break;
            default: {
                LOG_LIMIT(8, "video: %s ioctl %08x not handled\n", is_vic ? "vic" : "nvdec", cmd);
                break;
            }
        }
    else { LOG_LIMIT(8, "video: %s ioctl %08x not handled\n", is_vic ? "vic" : "nvdec", cmd); }
    pm_unlock(&g_hlk);
    return r;
}
