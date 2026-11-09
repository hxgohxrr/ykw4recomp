/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_PRESENT_H
#define SNX_CTR_PRESENT_H
#include <stdbool.h>
#include <stdint.h>

bool snx_ctr_present_init(const char* frame_dir);
bool snx_ctr_present_ready(void);

bool snx_ctr_present_windowed(void);

bool snx_ctr_present_frame(unsigned screen, unsigned width, unsigned height, unsigned format, unsigned pitch,
                           const unsigned char* pixels, uint64_t* hash);

bool snx_ctr_present_closed(void);
void snx_ctr_present_free(void);
#endif
