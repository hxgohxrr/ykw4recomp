/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_UI_H
#define SNX_UI_H
#include "common/cpu/aarch64/cpu.h"

typedef struct {
    float x, y, w, h;  
    u16 u, v, uw, vh;  
    u32 rgba;          
    u8 turn;           
    u8 sharp;          
} SnxUiQuad;
typedef struct {
    SnxUiQuad* q;
    int n, max;  
} SnxUiList;


void ui_sprite(SnxUiList* l, int sprite, float x, float y, float w, float h, u32 alpha, int turn);

void ui_fill(SnxUiList* l, float x, float y, float w, float h, u32 rgba);

float ui_text(SnxUiList* l, float x, float y, int scale, const char* s, u32 rgba);


void ui_atlas_set(const u32* sprites, int w, int h);
const u32* ui_atlas(int* w, int* h, u32* version);
void ui_atlas_done(void);


void ui_lock(void);
void ui_unlock(void);
#endif
