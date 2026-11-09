/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors
   The 3DS DSP in high level: the firmware's audio pipeline run on the host instead of the Teak's code. Every audio
   frame (160 samples at 32728 Hz) reads the configuration the application wrote into the current of the two shared
   memory regions in DSP RAM, plays the 24 sources' buffers out of FCRAM into the three intermediate mixes, mixes those
   into the final frame and writes the statuses back into the other region. Ported from Azahar
   audio_core/hle/{hle,source,mixers,filter,effects}.cpp and audio_core/{codec,interpolate}.cpp (GPL-2.0-or-later,
   Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project) at azahar-emu/azahar a4b335e. The AAC decoder
   on the binary pipe is not ported: a request on it stops (snx_ctr_dsp_pipe_write). */
#ifndef SNX_CTR_DSP_H
#define SNX_CTR_DSP_H
#include "3ds/memory/ctr_memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SNX_CTR_DSP_RATE     32728u  
#define SNX_CTR_DSP_SAMPLES  160u    

#define SNX_CTR_DSP_FRAME_NS 4888706u
#define SNX_CTR_DSP_RAM      0x1ff00000u  
#define SNX_CTR_DSP_PIPES    8u

enum { SNX_CTR_DSP_PIPE_DEBUG, SNX_CTR_DSP_PIPE_DMA, SNX_CTR_DSP_PIPE_AUDIO, SNX_CTR_DSP_PIPE_BINARY };
typedef enum { SNX_CTR_DSP_OFF, SNX_CTR_DSP_ON, SNX_CTR_DSP_SLEEPING } SnxCtrDspState;
typedef struct SnxCtrDsp SnxCtrDsp;

typedef void (*SnxCtrDspInterrupt)(void* context, unsigned pipe);

SnxCtrDsp* snx_ctr_dsp_new(SnxCtrMemory* memory, SnxCtrDspInterrupt interrupt, void* context);
void snx_ctr_dsp_free(SnxCtrDsp* dsp);
SnxCtrDspState snx_ctr_dsp_state(const SnxCtrDsp* dsp);

const char* snx_ctr_dsp_pipe_write(SnxCtrDsp* dsp, unsigned pipe, const uint8_t* data, size_t size);
size_t snx_ctr_dsp_pipe_readable(const SnxCtrDsp* dsp, unsigned pipe);

size_t snx_ctr_dsp_pipe_read(SnxCtrDsp* dsp, unsigned pipe, uint8_t* out, size_t size);

bool snx_ctr_dsp_tick(SnxCtrDsp* dsp, int16_t frame[SNX_CTR_DSP_SAMPLES][2]);
#endif
