/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_HID_MOTION_H
#define SNX_HID_MOTION_H
#include "nx/hle/services/hid.h"
#include "nx/hle/services/hid_layout.h"

typedef struct {
    float quaternion[4];  
    float angle[3];
    int was_valid;
} HidMotionState;


void hid_motion_sample(HidMotionState* state, const PadMotion* motion, u64 dt_ns, u64 sampling_number,
                       u8 out[HS_HidSixAxisSensorState]);
int hid_motion_at_rest(const PadMotion* motion);

#endif
