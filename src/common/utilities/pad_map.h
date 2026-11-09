/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_PAD_MAP_H
#define SNX_PAD_MAP_H
#include <stddef.h>


enum {
    SNX_PAD_SOUTH,
    SNX_PAD_EAST,
    SNX_PAD_WEST,
    SNX_PAD_NORTH,
    SNX_PAD_LB,
    SNX_PAD_RB,
    SNX_PAD_LT,
    SNX_PAD_RT,
    SNX_PAD_LS,
    SNX_PAD_RS,
    SNX_PAD_BACK,
    SNX_PAD_START,
    SNX_PAD_UP,
    SNX_PAD_DOWN,
    SNX_PAD_LEFT,
    SNX_PAD_RIGHT,
    SNX_PAD_CONTROLS
};
#define SNX_PAD_BUTTONS 16  
#define SNX_PAD_NONE    (-1)
#define SNX_PAD_UNSET   (-2)  

enum {
    SNX_PAD_LABEL_NONE,
    SNX_PAD_LABEL_A,
    SNX_PAD_LABEL_B,
    SNX_PAD_LABEL_X,
    SNX_PAD_LABEL_Y,
    SNX_PAD_LABEL_CROSS,
    SNX_PAD_LABEL_CIRCLE,
    SNX_PAD_LABEL_SQUARE,
    SNX_PAD_LABEL_TRIANGLE
};
typedef struct {
    signed char bit[SNX_PAD_CONTROLS];  
} SnxPadMap;


const char* snx_pad_control_key(int control);
const char* snx_pad_control_name(int control, const int* labels);

const char* snx_pad_button_name(int bit);

void snx_pad_map_position(SnxPadMap* map);

void snx_pad_map_swap_ab(SnxPadMap* map);

void snx_pad_map_unset(SnxPadMap* map);
void snx_pad_map_fill(SnxPadMap* map, const SnxPadMap* from);

int snx_pad_map_read(SnxPadMap* map, const char* text, char* error, size_t size);

void snx_pad_map_write(const SnxPadMap* map, char* out, size_t size);

int snx_pad_map_find(const SnxPadMap* map, int bit);
#define SNX_PAD_MAP_TEXT 192  
#endif
