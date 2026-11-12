# Frame pacing and rendering measurements

This build retains the Radeon fragment-flow fix. Profiling is opt-in and is not
an FPS unlock or a promise of higher performance. Normal scheduling, native
quality, synchronization and existing pipeline-cache defaults are unchanged.

## Tester run

1. Close the game and back up the executable. Use your own existing installation.
2. Replace only the executable, keeping its original name and DLLs. Keep the INI,
   game files, mods, translation and saves. No game-module rebuild is needed.
3. Select 60 FPS in the launcher and apply it. For the reference run use
   `[Debug] gpu_sync_trace=0` and `diag=0`. Remove `SNX_VK_VALIDATION` from the
   environment if set: even the value `0` enables validation in this runtime.
4. Set `[Debug] frame_profile=1` in the active game INI, or set the environment
   variable `SNX_FRAME_PROFILE=1` before opening the launcher. Only the exact
   environment value `1` enables this option; `0` does not enable it.
5. Repeat the affected transition, then remain in the same exterior for at least
   60 seconds. Record GPU, driver, CPU, resolution and whether VSync is enabled.
6. Send the game `.log` and `frame-profile.csv`. The exact CSV path and loaded INI
   are logged at startup. Do not send game files, keys, memory dumps or saves.
7. Disable `frame_profile` and unset `SNX_FRAME_PROFILE` for an uninstrumented
   comparison. The CSV is replaced at the next profiled run; save it first.

The Windows tester package includes `Start-Performance-Test.cmd`, which enables
profiling only for that launch and opens the normal launcher without extra
arguments. Place it beside the existing entry-point EXE. It does not change the
INI, force an FPS value, or enable the slow synchronization trace.

## What the report measures

Reports are written every five wall-clock seconds, including when no frames are
being produced, and a final partial report is attempted on normal game exit.
Each report logs the FPS limit actually loaded, VSync and synchronization mode.
The requested value `-1` means `game`; `0` means unlimited. Neither guarantees
that the game produces the requested number of frames.

- `draw_cpu` and `compute_cpu`: elapsed host time around command preparation,
  including lock contention, shader/pipeline work and any nested waits. These
  are not guest CPU execution time or CPU utilization. The existing Windows
  `profile=1` sampling profiler can be used separately for guest CPU investigation.
- `pipeline_graphics` and `pipeline_compute`: host pipeline-creation calls.
- `wait_graphics` and `wait_compute`: host timeline-semaphore waits.
- `submit_graphics`, `submit_compute`, `present_cpu`: host driver-call duration,
  not GPU execution time or the number of images physically displayed.
- `pacing`: actual time spent in the presentation limiter's sleep call.
- `guest_present_interval`: the interval requested by the guest, converted from
  console refresh ticks to nanoseconds. This is a request, not a measured delay.
- `host_tick_interval`: elapsed time between runtime event/VSync loop iterations;
  useful for checking whether the host loop itself is running late.
- `frame_interval`: intervals between completed guest image handoffs. The report
  FPS counts these handoffs, not monitor refreshes or unique visual content.
- `gpu_graphics_span`: timestamp spans for graphics-queue command buffers. This
  includes compute routed to that queue but does not measure the independent
  compute queue. Spans may overlap or include GPU stalls; their sum is neither
  GPU utilization nor frame time and must not be subtracted from host times.
- `upload_bytes`: bytes allocated from graphics upload staging, not all texture
  traffic or all transfers. `query_dropped` counts unavailable/unsampled GPU
  timestamp spans, including unsupported timestamps or bounded query capacity.

GPU queries are read without `WAIT_BIT` only after an existing frame-recycle
completion wait. No new fence, queue-idle or device-idle wait is introduced.
There are at most 128 timestamp pairs per frame-ring slot. Unsupported timestamps
disable GPU sampling without disabling the CPU report. Outstanding GPU samples
can lag reports or remain uncollected at exit. Query pools share the renderer's
device lifetime.

CSV counters are window deltas; `max_since_start_ns` is the lifetime maximum, not
a per-window maximum. Upload and dropped-query quantities use the `samples`
column, with zero duration. Concurrent counter snapshots have approximate window
boundaries. Nested CPU spans and graphics spans must not be added into one budget.
Instrumentation has overhead: compare the same scene with it disabled as well.

## Optimization gate

Use these results before changing synchronization, enabling optional descriptor
caches, moving pipeline creation to workers or changing the game's timing.
Measure the same save/scene, repeat both cold and warmed runs, and verify image
correctness, game speed and the previously failing Radeon transition. Do not
interpret a 31-32 FPS report alone as evidence of a particular bottleneck.
