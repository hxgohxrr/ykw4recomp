/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/gpu/maxwell/engines_int.h"


u8* bl_span_ptr(u64 base, u32 wbytes, u32 h, u32 bh, u32 z, u32 bd, u64* span) {
    static int off = -1;
    if (off < 0) off = getenv("SNX_DMA_SLOW") != NULL;
    if (off) return NULL;
    u64 gh = 8ull << bh, gd = 1ull << bd;
    u64 sz = (((u64)wbytes + 63) & ~63ull) * ((h + gh - 1) / gh * gh) * ((z / gd + 1) * gd);
    if (!sz || sz > (256ull << 20)) return NULL;
    u8* p = gmem_ptr(base, sz);
    if (p) *span = sz;
    return p;
}

static u8* dma_buf(u32 len) {
    u8* b = malloc(len ? len : 1);
    if (!b) WARN_LIMIT(1, "dma: no memory for a line of %u bytes, copy left out\n", len);
    return b;
}

static void dma_fill(u8* b, u32 len, u32 size, u32 value) {
    u32 i;
    for (i = 0; i < len; i++) b[i] = (u8)(value >> (8 * (i % size)));
}

typedef struct {
    u64 src, dst;
    u32 pitch_in, pitch_out, line_len, lines;  
    int src_linear, dst_linear, multi, remap, fill;
    u32 bpp, comp;  
} DmaLaunch;
static void dma_decode(const u32* r, u32 v, DmaLaunch* d) {
    d->src = ((u64)r[0x100] << 32) | r[0x101];
    d->dst = ((u64)r[0x102] << 32) | r[0x103];
    d->pitch_in = r[0x104];
    d->pitch_out = r[0x105];
    d->line_len = r[0x106];
    d->lines = r[0x107];
    d->src_linear = (v >> 7) & 1;
    d->dst_linear = (v >> 8) & 1;
    d->multi = (v >> 9) & 1;
    d->remap = (v >> 10) & 1;
    if (!d->multi) d->lines = 1;
    d->bpp = 1;
    d->comp = 1;

    d->fill = d->remap && (r[0x1C2] & 0x7777) == 0x4444;
    if (d->remap) {
        u32 rc = r[0x1C2];
        u32 csz = ((rc >> 20) & 3) + 1;
        d->comp = ((rc >> 16) & 3) + 1;
        d->bpp = d->comp * (d->fill ? ((rc >> 24) & 3) + 1 : csz);
        d->line_len *= d->bpp;
    }
    if (d->fill) d->src_linear = 1;  
}

static int dma_linear_source(const DmaLaunch* d, u64* src, u64* len) {
    if (!d->src_linear || d->fill || !d->lines || !d->line_len) return 0;
    *src = d->src;
    *len = (u64)d->pitch_in * (d->lines - 1) + d->line_len;
    return 1;
}


typedef struct {
    u64 src, len, gen;
    u64 job_id;
    u32 launch_index;
    u8* data;
} DmaSnap;
typedef struct {
    DmaSnap* q;  
    u32 cap, head, tail;
} DmaSnapRing;
#define SNAP_BYTES ((u64)256 << 20)  
static DmaSnapRing g_snap[128];      
static u64 g_snap_bytes;
static PMutex g_snap_lock = PMUTEX_INIT;

void dma_snap_launch(u32 channel, u64 job_id, u32 launch_index, const u32* r, u32 v) {
    DmaLaunch d;
    DmaSnap e;
    DmaSnapRing* g = &g_snap[channel & 127];
    dma_decode(r, v, &d);
    if (!dma_linear_source(&d, &e.src, &e.len)) return;
    e.data = NULL;
    e.job_id = job_id;
    e.launch_index = launch_index;
    e.gen = gpu_mem_generation();  
    pm_lock(&g_snap_lock);
    if (g_snap_bytes + e.len <= SNAP_BYTES) {
        g_snap_bytes += e.len;
        pm_unlock(&g_snap_lock);
        e.data = malloc(e.len);
        if (e.data) gmem_read_raw(e.src, e.data, e.len);
        pm_lock(&g_snap_lock);
        if (!e.data) g_snap_bytes -= e.len;
    }
    if (g->head - g->tail == g->cap) {  
        u32 cap = g->cap ? g->cap * 2 : 64, i;
        DmaSnap* q = malloc(sizeof *q * cap);
        if (!q) fatal("dma: out of memory for the copies queued on channel %u\n", channel & 127);
        for (i = 0; i < g->head - g->tail; i++) q[i] = g->q[(g->tail + i) % g->cap];
        free(g->q);
        g->q = q;
        g->head -= g->tail;
        g->tail = 0;
        g->cap = cap;
    }
    g->q[g->head++ % g->cap] = e;
    pm_unlock(&g_snap_lock);
}

static u8* dma_snap_take(u32 channel, u64 job_id, u32 launch_index, u64 src, u64 len) {
    DmaSnapRing* g = &g_snap[channel & 127];
    DmaSnap e;
    pm_lock(&g_snap_lock);
    while (g->tail != g->head && g->q[g->tail % g->cap].job_id < job_id) {
        DmaSnap* stale = &g->q[g->tail++ % g->cap];
        if (stale->data) g_snap_bytes -= stale->len;
        free(stale->data);
    }
    if (g->tail == g->head || g->q[g->tail % g->cap].job_id != job_id ||
        g->q[g->tail % g->cap].launch_index != launch_index) {
        pm_unlock(&g_snap_lock);
        return NULL;
    }
    e = g->q[g->tail++ % g->cap];
    if (e.data) g_snap_bytes -= e.len;
    pm_unlock(&g_snap_lock);
    if (e.src != src || e.len != len) {  
        WARN_LIMIT(1, "dma: channel %u's copy reads %llx+%llx, its submit read %llx+%llx\n", channel & 127,
                   (unsigned long long)src, (unsigned long long)len, (unsigned long long)e.src,
                   (unsigned long long)e.len);
        free(e.data);
        return NULL;
    }
    if (e.data) {
        gmem_need(src, len);  
        if (gpu_mem_dirty_since(e.gen, src, len)) {
            free(e.data);
            return NULL;
        }
    }
    return e.data;
}
void dma_snap_finish(u32 channel, u64 job_id) {
    DmaSnapRing* g = &g_snap[channel & 127];
    pm_lock(&g_snap_lock);
    while (g->tail != g->head && g->q[g->tail % g->cap].job_id <= job_id) {
        DmaSnap* stale = &g->q[g->tail++ % g->cap];
        if (stale->data) g_snap_bytes -= stale->len;
        free(stale->data);
    }
    pm_unlock(&g_snap_lock);
}
void dma_snap_reset(u32 channel) {
    DmaSnapRing* g = &g_snap[channel & 127];
    pm_lock(&g_snap_lock);
    while (g->tail != g->head) {
        DmaSnap* e = &g->q[g->tail++ % g->cap];
        if (e->data) g_snap_bytes -= e->len;
        free(e->data);
    }
    pm_unlock(&g_snap_lock);
}

static void src_read(const u8* snap, u64 src, u64 off, u8* dst, u32 n) {
    if (snap)
        memcpy(dst, snap + off, n);
    else
        gmem_read(src + off, dst, n);
}

void dma_method(GpuChannel* ch, u32 m, u32 v) {
    m &= 0x1FF;
    ch->rdma[m] = v;
    if (m != 0xC0) return;  
    u32* r = ch->rdma;
    DmaLaunch d;
    dma_decode(r, v, &d);
    u64 src = d.src, dst = d.dst;
    u32 pitch_in = d.pitch_in, pitch_out = d.pitch_out, line_len = d.line_len, lines = d.lines;
    int src_linear = d.src_linear, dst_linear = d.dst_linear, multi = d.multi, remap = d.remap, fill = d.fill;
    u32 bpp = d.bpp, comp = d.comp;

    u32 dbh = (r[0x1C3] >> 4) & 15, dw = r[0x1C4], dh = r[0x1C5], dd = r[0x1C6];
    (void)dd;
    u32 sbh = (r[0x1CA] >> 4) & 15, sw = r[0x1CB], sh = r[0x1CC];
    u32 sx = r[0x1CF] & 0xFFFF, sy = r[0x1CF] >> 16;
    u32 dx = r[0x1C8] & 0xFFFF, dy = r[0x1C8] >> 16;


    u32 dz = 0, sz_ = 0, dbd = 0, sbd = 0;
    if (!dst_linear && r[0x1C6] > 1) {
        dz = r[0x1C7];
        dbd = (r[0x1C3] >> 8) & 15;
    }
    if (!src_linear && r[0x1CD] > 1) {
        sz_ = r[0x1CE];
        sbd = (r[0x1CA] >> 8) & 15;
    }
    {
        u32 dl = dz ? 0 : r[0x1C7], sl = sz_ ? 0 : r[0x1CE];
        if (r[0x1C6] > 1) dl = 0;
        if (r[0x1CD] > 1) sl = 0;
        if (!dst_linear && dl) {
            u32 wbb = dw * (remap ? bpp : 1);
            u64 gh = 8u << dbh;
            u64 lsz = (((u64)wbb + 63) & ~63ull) * ((dh + gh - 1) / gh * gh);
            dst += lsz * dl;
        }
        if (!src_linear && sl) {
            u32 wbb = sw * (remap ? bpp : 1);
            u64 gh = 8u << sbh;
            u64 lsz = (((u64)wbb + 63) & ~63ull) * ((sh + gh - 1) / gh * gh);
            src += lsz * sl;
        }
    }
    if (!multi && (!src_linear || !dst_linear)) {  
        LOG_LIMIT(4, "dma: single-line copy with a block linear %s (launch %x), copied through the layout\n",
                  src_linear ? "destination" : "source", v);
    }
    LOG_LIMIT(
        6,
        "dma: %s->%s src=%llx dst=%llx pitch %u/%u len=%u lines=%u multi=%d remap=%d(%x) dst[bh=%u w=%u h=%u d=%u l=%u o=%x] src[bh=%u w=%u h=%u o=%x] launch=%x\n",
        src_linear ? "lin" : "bl", dst_linear ? "lin" : "bl", (unsigned long long)src, (unsigned long long)dst,
        pitch_in, pitch_out, line_len, lines, multi, remap, r[0x1C2], dbh, dw, dh, r[0x1C6], r[0x1C7], r[0x1C8], sbh,
        sw, sh, r[0x1CF], v);
    if (g_renderer_up) {
        g_renderer->rt_flush(src);
        g_renderer->rt_flush(dst);
    }
    u8* snap = NULL;
    {
        u64 ssrc, slen;
        if (dma_linear_source(&d, &ssrc, &slen))
            snap = dma_snap_take(ch->id, ch->dma_job_id, ch->dma_launch_index, ssrc, slen);
        ch->dma_launch_index++;
    }
    u8* dpp = NULL;
    u8* spp = NULL;
    u64 dspan = 0, sspan = 0;
    (void)spp;
    (void)sspan;
    if (src_linear && dst_linear) {
        u8* tmp = dma_buf(line_len);
        if (tmp && fill) dma_fill(tmp, line_len, comp, r[0x1C0]);
        if (tmp && !multi) {
            if (!fill) src_read(snap, src, 0, tmp, line_len);
            gmem_write(dst, tmp, line_len);
        } else if (tmp) {
            for (u32 y = 0; y < lines; y++) {
                if (!fill) src_read(snap, src, (u64)y * pitch_in, tmp, line_len);
                gmem_write(dst + (u64)y * pitch_out, tmp, line_len);
            }
        }
        free(tmp);
    } else if (src_linear && !dst_linear &&
               (dpp = bl_span_ptr(dst, dw * (remap ? bpp : 1), dh, dbh, dz, dbd, &dspan))) {

        u32 wb = dw * (remap ? bpp : 1);
        u8* buf = dma_buf(line_len);
        if (g_gpu_before_write) g_gpu_before_write(dst, dspan);
        if (buf && fill) dma_fill(buf, line_len, comp, r[0x1C0]);
        for (u32 y = 0; buf && y < lines; y++) {
            if (!fill) src_read(snap, src, (u64)y * pitch_in, buf, line_len);
            for (u32 x = 0; x < line_len;) {
                u32 xb = dx * (remap ? bpp : 1) + x, n = 16 - (xb & 15);
                if (n > line_len - x) n = line_len - x;
                u64 o = bl_offset(xb, dy + y, dz, wb, dh, dbh, dbd);
                if (o + n <= dspan)
                    memcpy(dpp + o, buf + x, n);
                else
                    gmem_write(dst + o, buf + x, n);
                x += n;
            }
        }
        free(buf);
        gpu_mem_written(dst, dspan);
    } else if (src_linear && !dst_linear) {
        u32 wb = dw * (remap ? bpp : 1);
        u8* buf = dma_buf(line_len);
        if (buf && fill) dma_fill(buf, line_len, comp, r[0x1C0]);
        for (u32 y = 0; buf && y < lines; y++) {
            if (!fill) src_read(snap, src, (u64)y * pitch_in, buf, line_len);

            for (u32 x = 0; x < line_len;) {
                u32 xb = dx * (remap ? bpp : 1) + x, n = 16 - (xb & 15);
                if (n > line_len - x) n = line_len - x;
                u64 o = bl_offset(xb, dy + y, dz, wb, dh, dbh, dbd);
                gmem_write(dst + o, buf + x, n);
                x += n;
            }
        }
        free(buf);
    } else if (!src_linear && dst_linear) {
        u32 wb = sw * (remap ? bpp : 1);
        u8* buf = dma_buf(line_len);
        for (u32 y = 0; buf && y < lines; y++) {
            for (u32 x = 0; x < line_len;) {
                u32 xb = sx * (remap ? bpp : 1) + x, n = 16 - (xb & 15);
                if (n > line_len - x) n = line_len - x;
                u64 o = bl_offset(xb, sy + y, sz_, wb, sh, sbh, sbd);
                gmem_read(src + o, buf + x, n);
                x += n;
            }
            gmem_write(dst + (u64)y * pitch_out, buf, line_len);
        }
        free(buf);
    } else if (!src_linear && !dst_linear &&
               (dpp = bl_span_ptr(dst, dw * (remap ? bpp : 1), dh, dbh, dz, dbd, &dspan)) &&
               (spp = bl_span_ptr(src, sw * (remap ? bpp : 1), sh, sbh, sz_, sbd, &sspan))) {
        u32 swb = sw * (remap ? bpp : 1), dwb = dw * (remap ? bpp : 1);
        if (g_gpu_before_write) g_gpu_before_write(dst, dspan);  
        for (u32 y = 0; y < lines; y++)
            for (u32 x = 0; x < line_len;) {
                u32 xs = sx * (remap ? bpp : 1) + x, xd = dx * (remap ? bpp : 1) + x, n = 16 - (xs & 15),
                    m = 16 - (xd & 15);
                if (m < n) n = m;
                if (n > line_len - x) n = line_len - x;
                u64 so = bl_offset(xs, sy + y, sz_, swb, sh, sbh, sbd),
                    dof = bl_offset(xd, dy + y, dz, dwb, dh, dbh, dbd);
                if (so + n <= sspan && dof + n <= dspan)
                    memmove(dpp + dof, spp + so, n);
                else {
                    u8 b[16];
                    gmem_read(src + so, b, n);
                    gmem_write(dst + dof, b, n);
                }
                x += n;
            }
        gpu_mem_written(dst, dspan);
    } else {
        u32 swb = sw * (remap ? bpp : 1), dwb = dw * (remap ? bpp : 1);
        for (u32 y = 0; y < lines; y++)
            for (u32 x = 0; x < line_len;) {
                u32 xs = sx * (remap ? bpp : 1) + x, xd = dx * (remap ? bpp : 1) + x, n = 16 - (xs & 15),
                    m = 16 - (xd & 15);
                if (m < n) n = m;
                if (n > line_len - x) n = line_len - x;
                u8 b[16];
                gmem_read(src + bl_offset(xs, sy + y, sz_, swb, sh, sbh, sbd), b, n);
                gmem_write(dst + bl_offset(xd, dy + y, dz, dwb, dh, dbh, dbd), b, n);
                x += n;
            }
    }
    free(snap);
    if (g_renderer_up) g_renderer->rt_reload(dst);

    u32 sem = (v >> 3) & 3;
    if (sem) {
        u64 sa = ((u64)r[0x90] << 32) | r[0x91];

        if (sem == 1) gpu_release_write(sa, &r[0x92], 4);
        if (sem == 2) {
            u32 q[4] = {r[0x92], 0, 0, 0};
            u64 t = gpu_ticks();
            memcpy(&q[2], &t, 8);
            gpu_release_write(sa, q, 16);
        }
    }
}
