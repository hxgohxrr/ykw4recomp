/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_HID_TOUCH_H
#define SNX_HID_TOUCH_H
#include "common/cpu/aarch64/cpu.h"

#define SNX_HID_TOUCHES      16    
#define SNX_HID_TOUCH_WIDTH  1280  
#define SNX_HID_TOUCH_HEIGHT 720


typedef struct {
    u32 down;  
    u32 x[SNX_HID_TOUCHES], y[SNX_HID_TOUCHES];
} TouchState;

typedef struct {
    u32 pressed, start, end;  
    u32 x[SNX_HID_TOUCHES], y[SNX_HID_TOUCHES];
} HidFingers;


void hid_touch_sample(HidFingers* fingers, const TouchState* seen, u64 sampling, u8* out);
#endif
