/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_AUDREN_DSP_H
#define SNX_AUDREN_DSP_H
#include "nx/memory/mem.h"

enum {
    SNX_AUDREN_WAVES = 4,      
    SNX_AUDREN_CHANNELS = 6,   
    SNX_AUDREN_MIX_OUTS = 24,  
    SNX_AUDREN_HISTORY = 8,    
    SNX_AUDREN_TEMP = 0x4000,  
};

typedef struct {
    u64 addr, size, ctx, ctx_size;
    i32 start, end, loop_start, loop_end, loop_count;
    u8 loop, ended, sent;  
} ARWave;


typedef struct {
    double x1, x2, y1, y2;
    i64 s0, s1;
} ARBiquad;


typedef struct {
    float mix[SNX_AUDREN_MIX_OUTS], prev_mix[SNX_AUDREN_MIX_OUTS];
    u8 in_use;
    u8 valid[SNX_AUDREN_WAVES];
    u32 index, consumed, offset;
    u64 played;
    i64 fraction;  
    i32 loop_count;
    u16 adpcm_header;
    i16 yn0, yn1;
    i16 history[SNX_AUDREN_HISTORY];
    ARBiquad biquad[2];
} ARChannel;

typedef struct {
    u8 in_use, fresh, format, quality, channels, flags;
    u8 play, last_play,
        was_playing;  
    u32 rate, wave_count, wave_index, flush, mix_id, splitter_id;
    u32 res[SNX_AUDREN_CHANNELS];
    float pitch, volume, prev_volume;
    u64 data, data_size;
    ARWave wave[SNX_AUDREN_WAVES];
    float bq_b[2][3], bq_a[2][2];  
    u8 bq_on[2], bq_was_on[2];
} ARVoice;

typedef struct {
    u8 in_use;
    float volume;
    u32 buffers, dest_splitter;
    i32 dest, offset, distance;
    float vol[SNX_AUDREN_MIX_OUTS][SNX_AUDREN_MIX_OUTS];
} ARMix;


typedef struct {
    float vol[SNX_AUDREN_MIX_OUTS], prev[SNX_AUDREN_MIX_OUTS];
    u32 mix_id;
    i32 next;  
    u8 in_use, need_update;
} ARDest;

typedef struct {
    i32 head;  
    u32 count;
} ARSplitter;

typedef struct {
    u8 type, enabled, fresh;
    u32 mix_id, order;
    u8 p[0xA0];  

    float avg[SNX_AUDREN_CHANNELS], gain[SNX_AUDREN_CHANNELS];
    float* look;
    u32 look_len, look_pos[SNX_AUDREN_CHANNELS];
    ARBiquad bq[SNX_AUDREN_CHANNELS];  
    void* fx;                          
} AREffect;

typedef struct {
    u8 type, in_use;
    u8 p[0x120];
    u32 pos, last, last2;  
} ARSink;

typedef struct {
    u32 rate, frame, buffers;  
    u32 nvoices, nres, nmix, neffects, nsinks, nsplit, ndest;
    ARVoice* voice;
    ARChannel* chan;
    ARMix* mix;
    AREffect* effect;
    ARSink* sink;
    ARSplitter* split;
    ARDest* dest;
    i32* stack;  
    u8* seen;    
    float*
        buf;    
    i16* temp;  
    i32* order;                         
    u64 frames;                         
    u32 rev;                            
    u32 warned;                         
    float lut4[3][512], lut8[3][1024];  
} ARState;

enum { SNX_AUDREN_STARTED = 0, SNX_AUDREN_STOPPED = 1, SNX_AUDREN_PAUSED = 2, SNX_AUDREN_STOPPING = 3 };
enum { SNX_AUDREN_PCM16 = 2, SNX_AUDREN_PCMF32 = 5, SNX_AUDREN_ADPCM = 6 };
enum {
    SNX_AUDREN_EFFECT_MIX = 1,
    SNX_AUDREN_EFFECT_AUX = 2,
    SNX_AUDREN_EFFECT_DELAY = 3,
    SNX_AUDREN_EFFECT_REVERB = 4,
    SNX_AUDREN_EFFECT_I3DL2 = 5,
    SNX_AUDREN_EFFECT_BIQUAD = 6,
    SNX_AUDREN_EFFECT_LIMITER = 7,
    SNX_AUDREN_EFFECT_CAPTURE = 8,
    SNX_AUDREN_EFFECT_COMPRESSOR = 9
};

enum { SNX_AUDREN_PARAM_INITIALIZED = 0, SNX_AUDREN_PARAM_UPDATING = 1, SNX_AUDREN_PARAM_UPDATED = 2 };
enum { SNX_AUDREN_SINK_DEVICE = 1, SNX_AUDREN_SINK_CIRCULAR = 2 };
#define SNX_AUDREN_FINAL_MIX  0
#define SNX_AUDREN_UNUSED_MIX 0x7FFFFFFF


ARState* audren_state_new(const u8* params);
void audren_state_free(ARState* s);

int audren_state_update(ARState* s, const u8* in, u64 in_size, u8* out, u64 out_size);

int audren_frame(ARState* s, i16* stereo);
#endif
