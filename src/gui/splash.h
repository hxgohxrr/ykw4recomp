/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_SPLASH_H
#define SNX_GUI_SPLASH_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;  
    const char* text;
    size_t size;
} SnxSplashSvg;
extern const SnxSplashSvg snx_splash_svgs[];  
extern const int snx_splash_svg_count;


typedef struct SnxSplashDrawing SnxSplashDrawing;
SnxSplashDrawing* snx_splash_open(const SnxSplashSvg* svg);

int snx_splash_draw(const SnxSplashDrawing* drawing, int side, void* pixels, int stride);
void snx_splash_close(SnxSplashDrawing* drawing);

#ifdef __cplusplus
}
#endif
#endif
