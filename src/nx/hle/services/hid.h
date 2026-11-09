/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_HID_H
#define SNX_HID_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/hle/services/hid_touch.h"
#include "nx/runtime/platform/plat.h"


typedef struct {
    float acceleration[3];
    float angular_velocity[3];
    int valid;
} PadMotion;

typedef enum { SNX_PAD_FULL_KEY = 0, SNX_PAD_JOY_DUAL, SNX_PAD_JOY_LEFT, SNX_PAD_JOY_RIGHT } PadKind;

typedef struct {
    u64 buttons;
    i32 lx, ly, rx, ry;
    int connected;        
    PadKind kind;         
    u32 source_id;        
    PadMotion motion[2];  

    u32 rumble[2];
} PadState;
extern PadState g_pad;
extern PadState g_pad_extra[7];  
extern TouchState g_touch;       
extern PMutex g_pad_input_lock;  
void hid_update(void);
int hid_connected_players(u32* first_id);

#endif
