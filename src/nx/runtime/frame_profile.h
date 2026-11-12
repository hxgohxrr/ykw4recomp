#ifndef YKW4_FRAME_PROFILE_H
#define YKW4_FRAME_PROFILE_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FP_DRAW_CPU, FP_COMPUTE_CPU, FP_PIPELINE_GRAPHICS, FP_PIPELINE_COMPUTE,
    FP_WAIT_GRAPHICS, FP_WAIT_COMPUTE, FP_SUBMIT_GRAPHICS, FP_SUBMIT_COMPUTE,
    FP_PRESENT_CPU, FP_PACING, FP_FRAME_INTERVAL, FP_GPU_GRAPHICS,
    FP_UPLOAD_BYTES, FP_QUERY_DROPPED, FP_GUEST_INTERVAL, FP_HOST_TICK, FP_METRICS
} FrameProfileMetric;

int frame_profile_init(int enabled, const char* path, uint64_t now, uint64_t frames);
int frame_profile_enabled(void);
uint64_t frame_profile_begin(void);
void frame_profile_end(FrameProfileMetric metric, uint64_t started);
void frame_profile_add(FrameProfileMetric metric, uint64_t duration, uint64_t units);
void frame_profile_frame(void);
int frame_profile_poll(uint64_t now, uint64_t frames, int fps_limit, int vsync, int sync_mode,
                       int force, char* output, size_t capacity);
int frame_profile_timestamp(uint64_t begin, uint64_t end, unsigned bits, double period, uint64_t* ns);
#endif
