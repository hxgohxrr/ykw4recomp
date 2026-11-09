/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_ETC2_H
#define SNX_ETC2_H
#include "common/cpu/aarch64/cpu.h"

enum {
    ETC2_RGB,      
    ETC2_RGB_PTA,  
    ETC2_RGBA,     
    EAC_R,         
    EAC_R_SNORM,   
    EAC_RG,        
    EAC_RG_SNORM,  
};


void etc2_decode_image(const u8* src, u32 w, u32 h, int kind, u8* dst);

u32 etc2_block_bytes(int kind);
u32 etc2_pixel_bytes(int kind);

#endif
