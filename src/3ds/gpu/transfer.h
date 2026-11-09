/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors
   The PICA200 transfer engine (memory fills, display transfers, texture copies), as Azahar's software blitter
   (video_core/renderer_software/sw_blitter.cpp, GPL-2.0-or-later) and register layout
   (video_core/pica/regs_external.h, GPL-2.0-or-later) at azahar-emu/azahar a4b335e define it.
   Copyright 2015-2026 Citra Emulator Project / Azahar Emulator Project.
   The engine works on host buffers the caller staged from guest memory: it never sees an address space, so every
   offset it computes is checked against the buffer it lands in. */
#ifndef SNX_CTR_TRANSFER_H
#define SNX_CTR_TRANSFER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum { SNX_CTR_PIX_RGBA8, SNX_CTR_PIX_RGB8, SNX_CTR_PIX_RGB565, SNX_CTR_PIX_RGB5A1, SNX_CTR_PIX_RGBA4 };

unsigned snx_ctr_pixel_bytes(unsigned format);

void snx_ctr_pixel_decode(unsigned format, const unsigned char* bytes, uint8_t rgba[4]);
void snx_ctr_pixel_encode(unsigned format, const uint8_t rgba[4], unsigned char* bytes);


#define SNX_CTR_FILL_24BIT (1u << 8)
#define SNX_CTR_FILL_32BIT (1u << 9)

void snx_ctr_fill(unsigned char* dst, size_t size, uint32_t value, uint32_t control);


#define SNX_CTR_XFER_FLIP        (1u << 0)
#define SNX_CTR_XFER_LINEAR      (1u << 1)  
#define SNX_CTR_XFER_CROP        (1u << 2)
#define SNX_CTR_XFER_TEXCOPY     (1u << 3)
#define SNX_CTR_XFER_NO_SWIZZLE  (1u << 5)
#define SNX_CTR_XFER_IN_FORMAT   8u   
#define SNX_CTR_XFER_OUT_FORMAT  12u  
#define SNX_CTR_XFER_SCALE_SHIFT 24u  

typedef enum {
    SNX_CTR_XFER_OK,
    SNX_CTR_XFER_EMPTY,        
    SNX_CTR_XFER_UNSUPPORTED,  
    SNX_CTR_XFER_BOUNDS        
} SnxCtrXferResult;


typedef struct {
    uint32_t input_size, output_size, flags;
} SnxCtrDisplayTransfer;

bool snx_ctr_display_spans(const SnxCtrDisplayTransfer* t, size_t* input_bytes, size_t* output_bytes);
SnxCtrXferResult snx_ctr_display_transfer(const SnxCtrDisplayTransfer* t, const unsigned char* in, size_t in_size,
                                          unsigned char* out, size_t out_size);


typedef struct {
    uint32_t size, input_line, output_line;
} SnxCtrTextureCopy;
bool snx_ctr_texture_spans(const SnxCtrTextureCopy* t, size_t* input_bytes, size_t* output_bytes);
SnxCtrXferResult snx_ctr_texture_copy(const SnxCtrTextureCopy* t, const unsigned char* in, size_t in_size,
                                      unsigned char* out, size_t out_size);


uint32_t snx_ctr_morton_offset(uint32_t x, uint32_t y, uint32_t bytes_per_pixel);
#endif
