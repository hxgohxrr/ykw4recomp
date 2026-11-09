# Performance changes, 20 October 2026

Reviewed upstream through 6a90d39, 253 commits since 3807fcf. This is a selective
backport: existing modules use ABI 8, while upstream has moved to ABI 12.

- 008b890: reuse descriptor sets by layout and descriptor contents within the frame.
- fb42732: record only changed dynamic state, pipeline and bindings. Clears/UI
  invalidate state, new command buffers/devices invalidate tags. Preserve local face-culling overrides.
- 9d5e9a8: wait for submissions producing requested buffer results. Unsubmitted results still finish fully.
- Conservative occlusion batches only ended queries, coalesces adjacent runs and
  emits one transfer-to-condition barrier per batch. Abandoned reservations never enter a wait.
  Old camera/frame, multi-draw and unknown spans remain visible.
- Skip proxy/sample-bound classification entirely when conservative culling is disabled.

The recent JIT default, separate recording thread, asynchronous pipelines and byte-range
storage-coherence changes need broader integration and were not transplanted indiscriminately.

180 FPS is a target (5.56 ms/frame), not a measured result or guarantee. A cap does not fix
engine timing. The launcher offers 180 without forcing existing settings. Preserve native
quality and safe defaults until controlled exterior tests establish a net culling benefit.
Raw test reports and logs are outside distribution, in workspace reports/ and
work/qa/perf-upstream-20261020/. Saves, launcher features and extracted-only preparation remain compatible.

The culling sampler pauses for 120 frames after 64 unrewarded retired tests and then probes again.
Retired counts affect only this effort budget: they never hide current-view objects.
SNX_VK_PERF_CACHE=0 disables the two draw caches for controlled diagnostic comparisons.

Measured YKW4 trials did not establish a frame-rate gain from the descriptor/dynamic caches;
they remain opt-in for this title (SNX_VK_PERF_CACHE=1), with unchanged legacy command recording by default.
The 180 cap is optional; existing FPS/vsync/quality settings are preserved.

## Coordinate investigation

A real triangle/query test on RTX 3060 produced 512/0 covered samples for clockwise/counterclockwise
with positive viewport height, and 0/512 after reflecting Y. This confirms the Vulkan mechanism,
not the root cause of all prior game rendering faults. No global axis flip is applied.
The register helper tests also cover negative Y swizzle, negative scale, origin and facing flags.
Backface culling and occlusion-query visibility are distinct; both can interact with a bad projection.
Vulkan defines these conventions independently of GPU vendor; Switch also uses NVIDIA Tegra hardware.
See the Vulkan VkViewport/rasterization specification, envytools gf100_3d.xml, and Mesa NVK's
nvk_emit_viewport/vk_to_nv9097_front_face for primary implementation references.

## Vulkan storage image limits, 25 October 2026

Pipeline checks now compare nonnegative shader image counts against Vulkan's
unsigned per-stage and per-set limits without narrowing the limits to `int`.
Values above INT_MAX previously became negative and rejected valid compute
pipelines, including ones requiring zero storage images. Fragment pipelines
used the same incorrect comparison. Startup logs now include both limits;
rejected compute pipelines log the requested count and the actual limits.
Asset-free regression cases cover UINT32_MAX, zero, exact capacity and overflow.

An RX 6600 report showed every compute dispatch skipped and a device loss while
leaving the initial house. The integer bug is confirmed by regression; its role
in that device loss still requires repeating the transition on the affected PC.
An unlimited presentation cap does not patch a game's own frame timing.

## GPU hang isolation

Set `gpu_sync_trace=1` under `[Debug]` in YKW4.ini and restart to log preparation,
shader hashes and completion of each draw/dispatch. The mode drains earlier work
before preparing each operation and waits for completion afterward. It changes
scheduling and is intentionally slow; it is off by default and is not a performance
fix. A missing completion marker narrows the failing work but does not prove that
the last recorded shader is defective. Restore `gpu_sync_trace=0` after testing.
Vulkan device loss now stops at the first checked failure instead of repeatedly
attempting descriptor allocation against a lost device. No TDR settings change.

Mode `gpu_sync_trace=2` is an opt-in experimental workaround: after submitting a
game frame, pending compute results land and graphics completion is waited for
before the frame ring advances. Draw/dispatch logging is disabled in this mode.
The same completed r12 RX 6600 trace contains about 417000 completed draws and an
ordinary exit, but cannot identify the original synchronization violation.
Mode 2 has not yet been verified on the affected Radeon transition and does not
claim stable FPS. Mode 0 preserves the normal scheduling; mode 1 remains available
for detailed isolation. A successful mode-2 test would narrow the problem without
establishing a specific barrier or lifetime fix.

### Dispatch-only isolation

The tester's RX 6600 log explicitly confirmed mode 2 at startup and on frame
completion, then reported device loss at 61.60 seconds during the reported door
transition. Frame-level waits are therefore insufficient for that case. The
strict mode 1 run completed without device loss; that single successful run
does not isolate a particular synchronization defect.

Mode `gpu_sync_trace=3` drains preceding compute and graphics work before each
dispatch and waits for that dispatch's completion, using the same dispatch
paths as mode 1. Draw preparation and submission retain normal scheduling.
Only dispatch markers are logged. This isolates dispatch-related scheduling
from the much more expensive per-draw serialization; it is not a verified fix.
The headless Vulkan fixture covers common initialization/presentation but does
not invoke the guest dispatch translation hook. A successful synthetic test
therefore does not establish that the door transition works on Radeon.

### Conservative GPU draw dependencies

Mode `gpu_sync_trace=4` closes any active dynamic rendering pass and records an
ALL_COMMANDS memory read/write dependency before each guest draw. Existing image
layout tracking remains responsible for layouts. This attempts to cover an
untracked within-queue graphics memory hazard without per-draw CPU completion
waits. It does not serialize independent queues or establish correct host-side
resource lifetimes. It may reduce performance and is opt-in, default 0 unchanged.

The RX 6600 mode 3 run still lost the device at 88.35 seconds after 5238 completed
dispatches. Its final dispatch marker is not proof of a defective compute shader.
A later unmodified-resource run also failed in normal mode. The root cause remains
unconfirmed; mode 4 requires the same door transition test on the affected GPU.
The headless sRGB/layer readback test now inserts the same dependency between
rendering operations and checks that attachment contents survive the pass breaks.

### Graphics-queue compute compatibility

The RX 6600 mode 4 report confirmed that draw dependencies were active, then
lost the device at 86.53 seconds. Conservative draw barriers did not resolve
the reported transition.

Mode `gpu_sync_trace=5` routes all guest compute dispatches through the existing
graphics-queue dispatch path, including programs with no textures or images.
Storage-buffer-only programs use graphics pending-output tracking rather than
the separate compute arena's asynchronous CPU writeback. Storage-image dispatches
retain the existing immediate graphics path. Queue families are still created
and logged normally; the routing message confirms which path guest dispatches
actually take. This avoids cross-queue guest compute work without per-draw CPU
completion waits or per-dispatch diagnostic logging. Default scheduling remains
mode 0. Performance and Radeon transition stability remain unverified.

The general headless Vulkan fixture validates initialization and presentation
with the new setting; it does not exercise the guest compute translation path.
