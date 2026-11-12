#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nx/runtime/frame_profile.h"
#include "nx/runtime/platform/plat.h"

static u64 clock_value = 100;
u64 plat_ns(void) { return clock_value; }

int main(int argc, char** argv) {
    assert(argc == 2);
    char output[4096];
    assert(frame_profile_init(0, argv[1], 100, 0));
    assert(!frame_profile_enabled() && !frame_profile_begin());
    frame_profile_add(FP_DRAW_CPU, 999, 1);
    assert(!frame_profile_poll(6000000100ull, 60, 60, 1, 0, 1, output, sizeof output));
    assert(frame_profile_init(1, argv[1], 100, 0));
    assert(frame_profile_enabled());
    u64 begin = frame_profile_begin();
    clock_value = 1100;
    frame_profile_end(FP_DRAW_CPU, begin);
    frame_profile_end(FP_DRAW_CPU, 0);
    frame_profile_add(FP_UPLOAD_BYTES, 0, 4096);
    frame_profile_add(FP_GPU_GRAPHICS, 12345, 1);
    frame_profile_add((FrameProfileMetric)999, 100, 1);
    frame_profile_frame();
    clock_value += 33000000;
    frame_profile_frame();
    assert(!frame_profile_poll(4000000100ull, 128, 60, 1, 0, 0, output, sizeof output));
    assert(frame_profile_poll(5000000100ull, 160, 60, 1, 0, 0, output, sizeof output));
    assert(strstr(output, "32.00 fps") && strstr(output, "fps_limit=60"));
    assert(strstr(output, "draw_cpu calls=1 total=0.001 ms"));
    assert(strstr(output, "upload_bytes=4096") && strstr(output, "gpu_graphics_span calls=1"));
    assert(strstr(output, "frame_interval calls=1 total=33.000 ms"));
    assert(frame_profile_poll(6000000100ull, 192, 0, 0, 2, 1, output, sizeof output));
    assert(!strstr(output, "draw_cpu") && strstr(output, "fps_limit=0 vsync=0 gpu_sync_trace=2"));
    uint64_t ns;
    assert(frame_profile_timestamp(250, 3, 8, 2.0, &ns) && ns == 18);
    assert(frame_profile_timestamp(UINT64_MAX - 3, 2, 64, 1.0, &ns) && ns == 6);
    assert(frame_profile_timestamp(100, 104, 64, 0.5, &ns) && ns == 2);
    assert(!frame_profile_timestamp(0, 1, 0, 1, &ns));
    assert(!frame_profile_timestamp(0, 1, 65, 1, &ns));
    assert(!frame_profile_timestamp(0, 1, 64, -1, &ns));
    assert(!frame_profile_timestamp(0, UINT64_MAX, 64, 2, &ns));
    assert(!frame_profile_timestamp(0, 1, 64, 1, NULL));
    char small[2] = {'x', 'x'};
    frame_profile_add(FP_DRAW_CPU, 100, 1);
    assert(frame_profile_poll(7000000100ull, 224, 60, 1, 0, 1, small, sizeof small));
    assert(small[1] == 0);
    assert(frame_profile_poll(8000000100ull, 256, 60, 1, 0, 1, NULL, 0));
    assert(frame_profile_init(0, NULL, 0, 0));
    FILE* file = fopen(argv[1], "rb");
    assert(file);
    char line[1024];
    assert(fgets(line, sizeof line, file) && strstr(line, "max_since_start_ns"));
    unsigned rows = 0;
    while (fgets(line, sizeof line, file)) rows++;
    assert(rows == FP_METRICS * 4);
    fclose(file);
    assert(!remove(argv[1]));
    assert(!frame_profile_init(1, NULL, 0, 0));
    assert(frame_profile_enabled());
    frame_profile_add(FP_DRAW_CPU, 100, 1);
    assert(frame_profile_poll(1, 0, -1, 1, 0, 1, output, sizeof output));
    assert(strstr(output, "draw_cpu calls=1"));
    frame_profile_init(0, NULL, 0, 0);
    return 0;
}
