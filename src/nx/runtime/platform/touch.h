/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_TOUCH_H
#define SNX_TOUCH_H
#include "nx/hle/services/hid_touch.h"

#define SNX_TOUCH_MOUSE (~(u64)0)  


void touch_picture(int cx, int cy, int cw, int ch, int picture[4]);

int touch_down(TouchState* t, u64* ids, u64 id, const int picture[4], float px, float py);

void touch_move(TouchState* t, const u64* ids, u64 id, const int picture[4], float px, float py);
void touch_up(TouchState* t, const u64* ids, u64 id);
#endif
