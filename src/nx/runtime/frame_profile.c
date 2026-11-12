#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nx/runtime/frame_profile.h"
#include "nx/runtime/platform/plat.h"

typedef struct { uint64_t calls, total, maximum; } Metric;
static Metric counters[FP_METRICS], previous[FP_METRICS];
static int enabled;
static FILE* csv;
static uint64_t first, last, last_frames, last_frame;
static const char* const names[FP_METRICS] = {
    "draw_cpu", "compute_cpu", "pipeline_graphics", "pipeline_compute",
    "wait_graphics", "wait_compute", "submit_graphics", "submit_compute",
    "present_cpu", "pacing", "frame_interval", "gpu_graphics_span",
    "upload_bytes", "query_dropped", "guest_present_interval", "host_tick_interval"
};

static void close_csv(void) {
    if (csv) fclose(csv);
    csv = NULL;
}

int frame_profile_init(int active, const char* path, uint64_t now, uint64_t frames) {
    close_csv();
    memset(counters, 0, sizeof counters);
    memset(previous, 0, sizeof previous);
    first = last = now;
    last_frames = frames;
    last_frame = 0;
    __atomic_store_n(&enabled, active != 0, __ATOMIC_RELAXED);
    if (!active) return 1;
    csv = path ? fopen(path, "w") : NULL;
    if (!csv) return 0;
    static int registered;
    if (!registered) { atexit(close_csv); registered = 1; }
    fputs("elapsed_s,window_s,frames,fps,metric,samples,total_ns,max_since_start_ns,fps_limit,vsync,gpu_sync_trace\n", csv);
    fflush(csv);
    return 1;
}

int frame_profile_enabled(void) { return __atomic_load_n(&enabled, __ATOMIC_RELAXED); }
uint64_t frame_profile_begin(void) { return frame_profile_enabled() ? plat_ns() : 0; }

void frame_profile_add(FrameProfileMetric metric, uint64_t duration, uint64_t units) {
    if (!frame_profile_enabled() || (unsigned)metric >= FP_METRICS) return;
    Metric* counter = &counters[metric];
    __atomic_add_fetch(&counter->total, duration, __ATOMIC_RELAXED);
    __atomic_add_fetch(&counter->calls, units, __ATOMIC_RELAXED);
    uint64_t maximum = __atomic_load_n(&counter->maximum, __ATOMIC_RELAXED);
    while (duration > maximum && !__atomic_compare_exchange_n(&counter->maximum, &maximum, duration, 0,
                                                              __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}

void frame_profile_end(FrameProfileMetric metric, uint64_t started) {
    if (!started) return;
    uint64_t now = plat_ns();
    if (now >= started) frame_profile_add(metric, now - started, 1);
}

void frame_profile_frame(void) {
    uint64_t now = frame_profile_begin();
    if (!now) return;
    uint64_t before = __atomic_exchange_n(&last_frame, now, __ATOMIC_RELAXED);
    if (before && now >= before) frame_profile_add(FP_FRAME_INTERVAL, now - before, 1);
}

int frame_profile_timestamp(uint64_t begin, uint64_t end, unsigned bits, double period, uint64_t* ns) {
    if (!bits || bits > 64 || !(period > 0) || !ns) return 0;
    uint64_t delta = end - begin;
    if (bits < 64) delta &= (UINT64_C(1) << bits) - 1;
    double duration = (double)delta * period;
    if (!(duration >= 0 && duration < 18446744073709551616.0)) return 0;
    *ns = (uint64_t)duration;
    return 1;
}

int frame_profile_poll(uint64_t now, uint64_t frames, int fps_limit, int vsync, int sync_mode,
                       int force, char* output, size_t capacity) {
    if (capacity) output[0] = 0;
    if (!frame_profile_enabled() || now <= last || (!force && now - last < UINT64_C(5000000000))) return 0;
    double elapsed = (double)(now - first) / 1e9, window = (double)(now - last) / 1e9;
    uint64_t produced = frames >= last_frames ? frames - last_frames : 0;
    double fps = (double)produced / window;
    int written = capacity ? snprintf(output, capacity,
        "perf: %.2f fps over %.2f s (%llu frames); fps_limit=%d vsync=%d gpu_sync_trace=%d\n",
        fps, window, (unsigned long long)produced, fps_limit, vsync, sync_mode) : 0;
    size_t used = written > 0 ? (size_t)written < capacity ? (size_t)written : capacity - 1 : 0;
    for (unsigned index = 0; index < FP_METRICS; index++) {
        Metric current = {
            __atomic_load_n(&counters[index].calls, __ATOMIC_RELAXED),
            __atomic_load_n(&counters[index].total, __ATOMIC_RELAXED),
            __atomic_load_n(&counters[index].maximum, __ATOMIC_RELAXED)
        };
        uint64_t calls = current.calls - previous[index].calls, total = current.total - previous[index].total;
        if (csv) fprintf(csv, "%.6f,%.6f,%llu,%.4f,%s,%llu,%llu,%llu,%d,%d,%d\n",
            elapsed, window, (unsigned long long)produced, fps, names[index], (unsigned long long)calls,
            (unsigned long long)total, (unsigned long long)current.maximum, fps_limit, vsync, sync_mode);
        if ((calls || total) && used < capacity) {
            written = index == FP_UPLOAD_BYTES || index == FP_QUERY_DROPPED ?
                snprintf(output + used, capacity - used, "perf: %s=%llu\n", names[index], (unsigned long long)calls) :
                snprintf(output + used, capacity - used, "perf: %s calls=%llu total=%.3f ms max_since_start=%.3f ms\n",
                    names[index], (unsigned long long)calls, (double)total / 1e6, (double)current.maximum / 1e6);
            if (written > 0) used += (size_t)written < capacity - used ? (size_t)written : capacity - used - 1;
        }
        previous[index] = current;
    }
    if (csv) fflush(csv);
    last = now;
    last_frames = frames;
    return 1;
}
