/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_PAD_SCREEN_H
#define SNX_PAD_SCREEN_H
#include "common/utilities/pad_map.h"

typedef struct {
    SnxPadMap own;   
    int swap_ab;     
    int labels[4];   
    int symmetric;   
    char pad[48];    
    char note[256];  
    unsigned held;   
    int focus;       
    int listening;   
} SnxPadScreen;

void snx_pad_screen_start(SnxPadScreen* screen);

void snx_pad_screen_map(const SnxPadScreen* screen, SnxPadMap* map);

enum { SNX_PAD_DID_NOTHING, SNX_PAD_DID_MAP, SNX_PAD_DID_LEAVE };
enum { SNX_PAD_KEY_UP, SNX_PAD_KEY_DOWN, SNX_PAD_KEY_LEFT, SNX_PAD_KEY_RIGHT, SNX_PAD_KEY_SELECT, SNX_PAD_KEY_BACK };

int snx_pad_screen_key(SnxPadScreen* screen, int key);

int snx_pad_screen_press(SnxPadScreen* screen, int control);

int snx_pad_screen_tap(SnxPadScreen* screen, int id);


enum {
    SNX_PAD_SHAPE_BOX,   
    SNX_PAD_SHAPE_TEXT,  
    SNX_PAD_SHAPE_BAR,   
    SNX_PAD_SHAPE_DISC   
};
enum {
    SNX_PAD_STYLE_PLAIN,  
    SNX_PAD_STYLE_SOFT,   
    SNX_PAD_STYLE_FOCUS,  
    SNX_PAD_STYLE_LIT,    
    SNX_PAD_STYLE_NOTE    
};
typedef struct {
    unsigned char kind, style;
    short id;  
    float x, y, w, h;
    const char* text;  
} SnxPadShape;
#define SNX_PAD_SHAPES 128
typedef struct {
    SnxPadShape shape[SNX_PAD_SHAPES];
    int n;
    char text[2048];
    int used;
} SnxPadDrawing;

void snx_pad_screen_lay(const SnxPadScreen* screen, float x, float y, float w, float h, float cw, float ch,
                        SnxPadDrawing* drawing);

int snx_pad_screen_hit(const SnxPadDrawing* drawing, float x, float y);
#endif
