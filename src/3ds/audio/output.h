/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors
   Where the DSP's frames go: the default playback device through SDL3, and a WAV recording of the first two minutes,
   like the presenter's frame snapshots, so an unattended run can be listened to afterwards. */
#ifndef SNX_CTR_AUDIO_OUTPUT_H
#define SNX_CTR_AUDIO_OUTPUT_H
#include "3ds/audio/dsp.h"
#include <stdbool.h>


bool snx_ctr_audio_init(const char* wav_path);

void snx_ctr_audio_frame(const int16_t frame[SNX_CTR_DSP_SAMPLES][2]);
void snx_ctr_audio_free(void);
#endif
