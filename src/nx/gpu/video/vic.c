/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/gpu/renderer.h"
#include "nx/gpu/video/host1x_int.h"

#define VIC_CFG_SIZE 0x610
#define VIC_NUM_SLOTS 8
#define VIC_MAX_ROW 0x4000  


typedef struct {
    u16 r, g, b, a;
} VicPixel;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }


static int sext20(u64 v) {
    u32 x = (u32)v & 0xFFFFF;
    return (x & 0x80000) ? (int)x - 0x100000 : (int)x;
}

static void matrix_read(VicSlot* sl, const u8* p) {
    u64 w0 = rd64(p, 0), w1 = rd64(p, 8), w2 = rd64(p, 16), w3 = rd64(p, 24);
    sl->mat_enable = (int)(w1 >> 63);
    sl->mat_shift = (u32)((w0 >> 60) & 15);
    sl->mat[0][0] = sext20(w0);
    sl->mat[1][0] = sext20(w0 >> 20);
    sl->mat[2][0] = sext20(w0 >> 40);
    sl->mat[0][1] = sext20(w1);
    sl->mat[1][1] = sext20(w1 >> 20);
    sl->mat[2][1] = sext20(w1 >> 40);
    sl->mat[0][2] = sext20(w2);
    sl->mat[1][2] = sext20(w2 >> 20);
    sl->mat[2][2] = sext20(w2 >> 40);
    sl->mat[0][3] = sext20(w3);
    sl->mat[1][3] = sext20(w3 >> 20);
    sl->mat[2][3] = sext20(w3 >> 40);
}


static u32 plane_rowbytes(u32 padded, u32 need, int blk) {
    if (padded >= need) return padded;
    return blk ? need : (need + 255) & ~255u;
}
static int plane_map(VicPlane* pl, u64 iova, u32 blk, u32 bh, u32 wbytes, u32 h) {
    pl->blk = blk;
    pl->bh = bh;
    pl->h = h;
    pl->pitch = wbytes;  
    if (blk) {

        u32 gy = 8u << bh;
        pl->lim = (u64)((wbytes + 63) & ~63u) * ((h + gy - 1) / gy * gy);
    } else {
        pl->lim = (u64)wbytes * h;
    }
    pl->p = iova ? iova_ptr(iova, pl->lim) : NULL;
    if (!pl->p && iova && blk) {

        pl->lim = (u64)wbytes * h;
        pl->p = iova_ptr(iova, pl->lim);
    }
    return pl->p != NULL;
}



static u8 g_rowy[VIC_MAX_ROW], g_rowc[VIC_MAX_ROW], g_rowo[VIC_MAX_ROW * 4];

static VicPixel g_slotrow[VIC_MAX_ROW];


static void plane_row(const VicPlane* pl, u32 xb, u32 y, u8* buf, u32 n, int write) {
    for (u32 k = 0; k < n;) {
        u32 x = xb + k, run = pl->blk ? 16 - (x & 15) : n - k;
        if (run > n - k) run = n - k;
        u64 off = pl->blk ? bl_offset(x, y, 0, pl->pitch, pl->h, pl->bh, 0) : (u64)y * pl->pitch + x;
        if (off + run <= pl->lim) {
            if (write)
                memcpy(pl->p + off, buf + k, run);
            else
                memcpy(buf + k, pl->p + off, run);
        } else if (!write)
            memset(buf + k, 0, run);
        k += run;
    }
}

static void pixels_convert(const VicSlot* sl, VicPixel* px, u32 n) {
    u32 i;
    if (!sl->mat_enable) return;
    int lo = (int)sl->clamp_lo, hi = (int)sl->clamp_hi;
    for (i = 0; i < n; i++) {
        int r = (int)px[i].r * sl->mat[0][0] + (int)px[i].g * sl->mat[0][1] + (int)px[i].b * sl->mat[0][2];
        int g = (int)px[i].r * sl->mat[1][0] + (int)px[i].g * sl->mat[1][1] + (int)px[i].b * sl->mat[1][2];
        int b = (int)px[i].r * sl->mat[2][0] + (int)px[i].g * sl->mat[2][1] + (int)px[i].b * sl->mat[2][2];
        r = ((r >> sl->mat_shift) + sl->mat[0][3]) >> 8;
        g = ((g >> sl->mat_shift) + sl->mat[1][3]) >> 8;
        b = ((b >> sl->mat_shift) + sl->mat[2][3]) >> 8;
        px[i].r = (u16)clampi(r, lo, hi);
        px[i].g = (u16)clampi(g, lo, hi);
        px[i].b = (u16)clampi(b, lo, hi);
    }
}

static void compose_cpu(const VicSlot* sl, u32 fmt, const VicPlane* oy, const VicPlane* oc) {
    u32 w = sl->w, h = sl->h, sx0 = sl->sx0, sy0 = sl->sy0, dx0 = sl->dx0, dy0 = sl->dy0;
    u32 wc = (w + 1) & ~1u;  
    int swap = fmt == 0x20;  
    for (u32 y = 0; y < h; y++) {

        plane_row(&sl->iy, sx0, sy0 + y, g_rowy, w, 0);
        u32 csx = sx0 & ~1u, cend = (sx0 + w + 1) & ~1u;
        plane_row(&sl->ic, csx, (sy0 + y) / 2, g_rowc, cend - csx, 0);
        for (u32 k = 0; k < w; k++) {
            u32 cb = ((sx0 + k) & ~1u) - csx;
            g_slotrow[k].r = (u16)(g_rowy[k] << 2);
            g_slotrow[k].g = (u16)(g_rowc[cb] << 2);
            g_slotrow[k].b = (u16)(g_rowc[cb + 1] << 2);
            g_slotrow[k].a = (u16)sl->alpha;
        }
        pixels_convert(sl, g_slotrow, w);
        u32 out_y = dy0 + y;
        if (fmt != 0x44) {

            for (u32 k = 0; k < w; k++) {
                u8* o = g_rowo + k * 4;
                u8 rr = (u8)(g_slotrow[k].r >> 2), gg = (u8)(g_slotrow[k].g >> 2);
                u8 bb = (u8)(g_slotrow[k].b >> 2), aa = (u8)(g_slotrow[k].a >> 2);
                o[0] = swap ? bb : rr;
                o[1] = gg;
                o[2] = swap ? rr : bb;
                o[3] = aa;
            }
            plane_row(oy, dx0 * 4, out_y, g_rowo, w * 4, 1);
        } else {

            for (u32 k = 0; k < w; k++) g_rowo[k] = (u8)(g_slotrow[k].r >> 2);
            plane_row(oy, dx0, out_y, g_rowo, w, 1);

            if (oc->p && !(out_y & 1)) {  
                for (u32 k = 0; k < wc; k += 2) {
                    g_rowo[k] = (u8)(g_slotrow[k].g >> 2);
                    g_rowo[k + 1] = (u8)(g_slotrow[k].b >> 2);
                }
                plane_row(oc, dx0, out_y / 2, g_rowo, wc, 1);
            }
        }
    }
}



static int slot_prepare(const u32* r, const u8* cs, int s, u32 sw, u32 sh, u32 fmt, VicSlot* out) {
    const u8* sl = cs + 0x90 + s * 0xB0;
    u64 sc = rd64(sl, 0x40);
    u32 ifmt = (u32)(sc & 0x7F), iblk = (u32)((sc >> 11) & 15), ibh = (u32)((sc >> 15) & 15);
    u32 iw = (u32)((sc >> 32) & 0x3FFF) + 1, ih = (u32)((sc >> 46) & 0x3FFF) + 1;
    if (ifmt != 0x44) {
        LOG_LIMIT(4, "video: VIC slot %d input format %x isn't handled (NV12 is), slot skipped\n", s, ifmt);
        return 0;
    }
    u64 src_x = rd64(sl, 0x20), src_y = rd64(sl, 0x28), dst = rd64(sl, 0x30);
    u32 sx0 = (u32)(src_x & 0x3FFFFFFF) >> 16, sx1 = (u32)((src_x >> 32) & 0x3FFFFFFF) >> 16;
    u32 sy0 = (u32)(src_y & 0x3FFFFFFF) >> 16, sy1 = (u32)((src_y >> 32) & 0x3FFFFFFF) >> 16;
    u32 dx0 = (u32)(dst & 0x3FFF), dx1 = (u32)((dst >> 16) & 0x3FFF);
    u32 dy0 = (u32)((dst >> 32) & 0x3FFF), dy1 = (u32)((dst >> 48) & 0x3FFF);
    if (sx1 < sx0 || sy1 < sy0 || dx1 < dx0 || dy1 < dy0 || sx0 >= iw || sy0 >= ih || dx0 >= sw || dy0 >= sh) {
        LOG_LIMIT(4, "video: VIC slot %d rectangles are empty or outside the surfaces, slot skipped\n", s);
        return 0;
    }
    u32 w = sx1 - sx0 + 1, h = sy1 - sy0 + 1;  
    u32 dw = dx1 - dx0 + 1, dh = dy1 - dy0 + 1;  
    if (w != dw || h != dh)
        LOG_LIMIT(2, "video: VIC slot %d scales %ux%u to %ux%u, which isn't done: the picture is cropped\n", s, w, h,
                  dw, dh);
    if (w > dw) w = dw;
    if (h > dh) h = dh;

    u32 tgt0 = rd32(cs, 0x18), tgt1 = rd32(cs, 0x1C);
    u32 tx0 = tgt0 & 0x3FFF, tx1 = (tgt0 >> 16) & 0x3FFF, ty0 = tgt1 & 0x3FFF, ty1 = (tgt1 >> 16) & 0x3FFF;
    if (tx1 < tx0 || ty1 < ty0) {
        LOG_LIMIT(4, "video: VIC slot %d target rectangle is empty, slot skipped\n", s);
        return 0;
    }
    if (dx0 > tx1 || dy0 > ty1) {
        LOG_LIMIT(4, "video: VIC slot %d rectangles are outside the target rectangle, slot skipped\n", s);
        return 0;
    }
    if (dx0 < tx0) {
        u32 cut = tx0 - dx0;
        if (cut >= w) {
            LOG_LIMIT(4, "video: VIC slot %d rectangles are outside the target rectangle, slot skipped\n", s);
            return 0;
        }
        sx0 += cut;
        dx0 += cut;
        w -= cut;
    }
    if (dy0 < ty0) {
        u32 cut = ty0 - dy0;
        if (cut >= h) {
            LOG_LIMIT(4, "video: VIC slot %d rectangles are outside the target rectangle, slot skipped\n", s);
            return 0;
        }
        sy0 += cut;
        dy0 += cut;
        h -= cut;
    }

    if (sx0 >= iw || sy0 >= ih || dx0 >= sw || dy0 >= sh) {
        LOG_LIMIT(4, "video: VIC slot %d rectangles are empty or outside the surfaces, slot skipped\n", s);
        return 0;
    }
    if (w > tx1 - dx0 + 1) w = tx1 - dx0 + 1;
    if (h > ty1 - dy0 + 1) h = ty1 - dy0 + 1;
    if (w > sw - dx0) w = sw - dx0;
    if (h > sh - dy0) h = sh - dy0;
    if (w > iw - sx0) w = iw - sx0;
    if (h > ih - sy0) h = ih - sy0;
    if (!w || !h) {
        LOG_LIMIT(4, "video: VIC slot %d rectangles are empty or outside the surfaces, slot skipped\n", s);
        return 0;
    }

    if (fmt == 0x44) {
        sx0 &= ~1u;
        sy0 &= ~1u;
        dx0 &= ~1u;
        dy0 &= ~1u;
    }
    u64 cfg2 = rd64(sl, 0x10);
    out->alpha = (u32)((cfg2 >> 32) & 0x3FF);
    out->clamp_lo = (u32)(cfg2 & 0x3FF);
    out->clamp_hi = (u32)((cfg2 >> 10) & 0x3FF);
    matrix_read(out, sl + 0x60);

    u64 pad = rd64(sl, 0x48);
    u32 ilw = (u32)(pad & 0x3FFF) + 1, ilh = (u32)((pad >> 14) & 0x3FFF) + 1;
    u32 icw = (u32)((pad >> 32) & 0x3FFF) + 1, ich = (u32)((pad >> 46) & 0x3FFF) + 1;
    u32 yrows = ilh > ih ? ilh : ih, crows = ich > (ih + 1) / 2 ? ich : (ih + 1) / 2;
    u64 ly = (u64)r[(0x400 + s * 0x60) / 4] << 8, lc = (u64)r[(0x404 + s * 0x60) / 4] << 8;
    if (!plane_map(&out->iy, ly, iblk, ibh, plane_rowbytes(ilw, iw, iblk), yrows) ||
        !plane_map(&out->ic, lc, iblk, ibh, plane_rowbytes(icw * 2, (iw + 1) & ~1u, iblk), crows)) {
        LOG_LIMIT(4, "video: VIC slot %d input (luma iova %llx, chroma %llx) not mapped, slot skipped\n", s,
                  (unsigned long long)ly, (unsigned long long)lc);
        return 0;
    }
    out->sx0 = sx0;
    out->sy0 = sy0;
    out->dx0 = dx0;
    out->dy0 = dy0;
    out->w = w;
    out->h = h;
    out->on = 1;
    LOG_LIMIT(3, "video: VIC slot %d: NV12 %ux%u %s (block height %u), source %u,%u, target %u,%u, %ux%u copied\n", s,
              iw, ih, iblk ? "block linear" : "pitch linear", 1u << ibh, sx0, sy0, dx0, dy0, w, h);
    return 1;
}

void vic_execute(const u32* r) {
    {
        static u32 nv;
        nv++;
        if (nv % 120 == 0) LOG("video: %u VIC jobs so far\n", nv);
    }
    u64 ly = (u64)r[0x720 / 4] << 8, lc = (u64)r[0x724 / 4] << 8, cfg_iova = (u64)r[0x708 / 4] << 8;
    u8* cs = iova_ptr(cfg_iova, VIC_CFG_SIZE);
    if (!cs) {

        LOG_LIMIT(4, "video: VIC config at iova %llx not mapped, job skipped\n", (unsigned long long)cfg_iova);
        return;
    }
    u64 cfg = rd64(cs, 0x20);
    u32 fmt = (u32)(cfg & 0x7F), blk = (u32)((cfg >> 11) & 15), bh = (u32)((cfg >> 15) & 15);
    u32 sw = (u32)((cfg >> 32) & 0x3FFF) + 1, sh = (u32)((cfg >> 46) & 0x3FFF) + 1;

    u64 planes = rd64(cs, 0x28);
    u32 lw = (u32)(planes & 0x3FFF) + 1, lh = (u32)((planes >> 14) & 0x3FFF) + 1;
    u32 ocw = (u32)((planes >> 32) & 0x3FFF) + 1, och = (u32)((planes >> 46) & 0x3FFF) + 1;
    if (lw < sw) sw = lw;
    if (lh < sh) sh = lh;

    static int fmt_env = -1;
    if (fmt_env == -1) {
        const char* e = getenv("SNX_VIC_FMT");
        fmt_env = e ? (int)(strtoul(e, 0, 0) & 0x7F) : -2;
    }
    if (fmt_env >= 0) fmt = (u32)fmt_env;
    if (fmt != 0x1F && fmt != 0x20 && fmt != 0x23 && fmt != 0x44) {
        LOG_LIMIT(4, "video: VIC output format %x isn't handled, job skipped\n", fmt);
        return;
    }
    LOG_LIMIT(3, "video: VIC config %016llx: format %x, %s (block height %u), surface %ux%u\n", (unsigned long long)cfg,
              fmt, blk ? "block linear" : "pitch linear", 1u << bh, sw, sh);

    u32 bpp = fmt == 0x44 ? 1 : 4;
    u32 oyrows = lh > sh ? lh : sh, ocrows = och > (sh + 1) / 2 ? och : (sh + 1) / 2;
    VicJob job = {0};
    job.fmt = fmt;
    job.sw = sw;
    job.sh = sh;
    if (!plane_map(&job.oy, ly, blk, bh, plane_rowbytes(lw * bpp, sw * bpp, blk), oyrows)) {
        LOG_LIMIT(4, "video: VIC output at iova %llx not mapped\n", (unsigned long long)ly);
        return;
    }
    if (fmt == 0x44 && !plane_map(&job.oc, lc, blk, bh, plane_rowbytes(ocw * 2, (sw + 1) & ~1u, blk), ocrows))
        LOG_LIMIT(4, "video: VIC output chroma at iova %llx not mapped, the picture has no color\n",
                  (unsigned long long)lc);

    int any = 0;
    for (int s = 0; s < VIC_NUM_SLOTS; s++)
        if (rd64(cs, 0x90 + s * 0xB0) & 1) any |= slot_prepare(r, cs, s, sw, sh, fmt, &job.slots[s]);
    if (!any) {
        LOG_LIMIT(4, "video: VIC job with no slot it could compose\n");
        return;
    }
    if (g_renderer_up && g_renderer->vic_compose(&job)) return;  
    for (int s = 0; s < VIC_NUM_SLOTS; s++)
        if (job.slots[s].on) compose_cpu(&job.slots[s], fmt, &job.oy, &job.oc);
}
