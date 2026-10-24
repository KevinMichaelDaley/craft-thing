# Bounded Vulkan device probe

## Exact solver and MPM comparisons

`DC_MPM_FUSED_GRID=1` combines particle-to-grid, grid solve and same-node water
feedback within each of the two unchanged MPM substeps. It remains optional;
four GPU fixtures compare all grid, force, velocity and particle bytes against
the separate stages, including water, obstacles, two layers and missing pages.

`solver_capacity_tests` compares the optimized solver against its frozen address
access paths before and after dynamic queue growth. GPU counters verify two
layout-capacity queries per invocation and no generic tagged address decoding
for known contact and body-state accesses. This does not limit body capacity.

Pressure batches cache five words of terrain/face coefficients per cell, while
retaining the original divergence expression and every red/black sweep. The
cache refreshes at each batch so intervening brush edits are respected. The
`pressure_work_tests` fixture compares pressure, velocity and cell bytes against
`DC_FLUID_VARIANTS=generic`, including mixed materials and partial hydraulic faces.

The saved-scene ordinary Wayland benchmark after these address changes measured
32.414 ms average and 38.887 ms worst over 500 real-time frames at 480x270 visible
physics/render resolution (640x448 with halo), with repeated air brush input.
The full-frame target remains unmet; kernel counters are not frame acceptance.

## Optional transport comparisons

`DC_TRANSPORT_LONG_SCAN=1` and `DC_TRANSPORT_PAGE_CACHE=1` select existing
cooperative long-run transport and page-cache specializations independently.
Both remain off by default. Four pipeline-wiring cases and the exact mass/color
fixture pass. Displayed fixed-clock tree-cut comparisons measured 29.434 ms
reference, 29.194 ms long-run, 29.539 ms page-cache and 29.692 ms combined average
over 300 frames; none met the worst-frame gate. These results do not establish
a runtime speedup. Final acceptance uses real elapsed time without a fixed clock.

## Saved-body support replay

`make build/diagnostics/body_support` builds an in-memory replay of a versioned
world snapshot. Run `./tools/with-vulkan build/diagnostics/body_support
PATH/rigid_bodies.bin` with no other GPU workloads. It restores the snapshot's
origin and its central 640x448 resident window, reports eight physics ticks,
and captures each solver substep's contact witnesses for the first automatically
extracted body, plus all bodies' awake states, rest flags, and overflow counters.
`build/support_scene.bmp` shows the resulting albedo.
The terrain inventory lists all rigid-capable components containing wood,
including their original bonds, release flags and unknown neighbors. The body
inventory includes size and wood-pixel counts. `support_fixed_wood.bmp` marks
remaining wood terrain magenta; `support_tree_detail.bmp` enlarges the tree area
of the level-piece scene so static branch pixels can be distinguished from the
entire moving trunk/crown.
Optional pairs of world X/Y coordinates erase those cells in the replay before
stepping. The tool never saves over the input world. This distinguishes actual
terrain supports from stale sleep flags, cold-page freezing, and solver rollback.
Set `DC_SUPPORT_PROP=assets/props/level_piece01.prop/asset.cprop` to exercise
the scene's old-save corner-bond migration before stepping, without saving it.
Set `DC_SUPPORT_FULL_TICKS=1` to include fluid and granular simulation rather
than stepping rigid bodies alone. Use this when investigating moving soil or
particles caught inside a body.

## Device health probe

Run from the repository root, with no other GPU tests running:

```sh
mkdir -p build/diagnostics
./tools/with-vulkan cc -std=c11 -O2 -Wall -Wextra -Werror tools/diagnostics/vulkan_probe.c -o build/diagnostics/vulkan_probe -lvulkan
./tools/with-vulkan glslangValidator -V --target-env vulkan1.1 -S comp -o build/diagnostics/vulkan_probe.comp.spv tools/diagnostics/vulkan_probe.comp
./tools/with-vulkan spirv-val --target-env vulkan1.1 build/diagnostics/vulkan_probe.comp.spv
./tools/with-vulkan env DC_VK_VALIDATE=1 timeout -k 2 12 build/diagnostics/vulkan_probe > build/diagnostics/recovery-probe.log 2>&1
cat build/diagnostics/recovery-probe.log
```

The checked-in shader writes exactly 16 words with one 16-invocation workgroup.
The probe first checks an empty queue fence, then a 16-word buffer fill, then the
compute result. Each fence has a one-second deadline; the process deadline also
bounds driver initialization and pipeline compilation. Vulkan 1.1 shader output
avoids requiring optional shader features for this basic health check.

A failed small submission means further GPU tests should stop. Killing a process
does not guarantee its submitted GPU work has stopped. This tool never resets the
driver, restarts the display, or submits physics/native-resolution workloads.

Preserve the log and current-boot kernel evidence before another reboot:

```sh
journalctl -k -b --no-pager --grep='i915|GPU HANG|wedged|Fence expiration|Resetting' > build/diagnostics/kernel-after-probe.log
```

`component_dispatch_tests` separately inspects poisoned indirect-dispatch metadata
using only one directly dispatched initialization workgroup. It never launches
the poisoned indirect counts. Its source also reuses this minimal device fixture,
avoiding initialization of all engine pipelines during this safety regression.

`gpu_frequency.py` records read-only Intel frequency samples alongside a bounded
command. It writes JSON lines under the supplied persistent output path and
inherits the command's stdout/stderr and exit status. It never writes sysfs or
changes clocks. Sampling adds host work, so use it to investigate frequency
behavior rather than as the authoritative frame-time benchmark:

Add `--drm-clients` to record cumulative per-client engine nanoseconds once per
second. Clients shared by multiple descriptors/processes are counted once per
PCI device and DRM client ID. Differences between samples expose competing GPU
work without changing other applications. Use `--interval .1` for displayed
comparisons; final frame acceptance should also run without the sampler.

```sh
python3 tests/tools/gpu_frequency_tests.py
DC_COLD_DIAG=1 ./tools/with-vulkan python3 tools/diagnostics/gpu_frequency.py \
  --output build/diagnostics/cold-frequency.jsonl -- \
  timeout --signal=TERM --kill-after=3s 30s ./build/pixel_performance_tests
```

The explicit process deadline is still required. A timed-out GPU command does
not prove its GPU work was cancelled; follow the probe failure procedure above.

`build/diagnostics/native360_frame_trace` links explicit timestamp wrappers around
the unchanged native benchmark's displayed frames. It reports both physics
steps' rigid, scheduled fluid-phase and MPM GPU totals, final candidate/contact
counts, and wall time per frame. `DC_NATIVE_SUSTAINED=1` selects the same
180-frame regression used by the normal benchmark. The extra query allocation,
timestamps, readback and logging belong only to this diagnostic binary; use the
ordinary native benchmark to establish final performance acceptance:

```sh
./tools/with-vulkan make build/diagnostics/native360_frame_trace
SDL_VIDEODRIVER=wayland DC_NATIVE_SUSTAINED=1 ./tools/with-vulkan \
  timeout --signal=TERM --kill-after=3s 45s \
  ./build/diagnostics/native360_frame_trace concave
```

Use validation for query correctness and disable it for timing comparisons.

`build/diagnostics/quarter_app_frame_trace` runs the actual material-placement
application, including saved scenes, with the same timestamp and Vulkan API
wrappers. Use a fresh copy of a saved world for each comparison. The original
world must remain untouched. Quarter-native visible physics and rendering are
both 480×270; the 640×448 active region also includes chunk rounding and the
offscreen simulation halo.

`DC_FRAME_TRACE_COARSE=1` keeps reusable command streams enabled and reports
whole-stage GPU timings. Add `DC_FRAME_TRACE_COMPONENTS=1` to inspect extraction
dispatches specifically. API rows separately report pipeline creation, memory
allocation, image acquisition, presentation, fence waits, queue/device idle and
body-resource servicing. These counters can overlap and must not be summed.
On Linux, `DC_FRAME_TRACE_IDLE_STACKS=1` prints call stacks for warm idle waits
longer than 1 ms.

`DC_FRAME_TIMINGS=1` works on the ordinary application with
`--benchmark-frames`. It prints total application frame time plus input,
preparation, painting, presentation and title-update times. This distinguishes
application stalls from time inside the GPU presentation function. Logging and
query instrumentation can perturb timing; use the ordinary application with
these flags unset for final performance acceptance.

Water transport normally recomputes each recipient's request instead of keeping
a separate shared-memory request array. Each lane owns disjoint recipients and
reads a recipient before its own write, preserving the original request and
priority. `DC_TRANSPORT_REQUESTS=cached` selects the reference path.
`DC_TRANSPORT_DRY_SCAN=0` disables the exact dry-line cache bypass. The experimental
`DC_SOLVER_FUSED_GRAIN=1` path remains opt-in because its displayed measurements
did not improve on the separate body/grain application passes.

`DC_CONTACT_PAIR_PROFILE=1` additionally freezes displayed frame 72 and measures
each existing broadphase candidate separately. This explicit diagnostic copies
and restores all touched body storage and backs up/restores device-only activity
storage on the GPU. It never runs a CPU physics solve. Candidate count is bounded
to 64, and the command still requires its external deadline. This extra work
perturbs frame timing; use its per-candidate results for attribution, not FPS
acceptance. Run with synchronization validation before using those timings.

The sustained native 360p regression runs 180 measured frames for each unchanged
dry, dense-fluid/grain and exact-concave-body workload. Both physics and display
are 640×360, with two fixed 60 Hz ticks per frame. Run this timing target without
validation; the original shorter benchmark remains available separately:

```sh
SDL_VIDEODRIVER=wayland ./tools/with-vulkan make test_native_360_sustained_performance
```

Set `DC_FLUID_DISPATCH_TRACE=1` on the explicit
`build/diagnostics/native360_fluid_trace` binary to capture its post-measurement
fluid dispatches for dry or dense workloads as well as concave bodies. This does
not enable hooks in the ordinary benchmark or change the measured frame loop.

Solver command recording is reused through secondary Vulkan command buffers.
These cache commands, never simulation results or indirect work counts. Origins,
body/queue growth, structural mode, and descriptor updates invalidate recordings.
Use `DC_SOLVER_COMMAND_CACHE=0` for the original recording path, or
`DC_MPM_COMMAND_CACHE=0` to disable only MPM command reuse. Set
`DC_COMMAND_CACHE_STATS=1` to print recording/execution counts at shutdown.
Detailed `quarter_app_frame_trace` queries disable command reuse so each frame
gets fresh per-dispatch query IDs; establish final frame-time acceptance with
the ordinary displayed application.

Set `DC_FRAME_TRACE_COARSE=1` on `quarter_app_frame_trace` to retain reusable
command streams and measure their enclosing rigid, MPM, fluid-phase, renderer,
and extraction stages. Queries remain in the primary command buffer; cached
secondaries never contain query IDs. Detailed kernel fields are zero for cached
programs in this mode. This reduces diagnostic disturbance but still requires
normal displayed application measurements for final acceptance.

`DC_FRAME_TRACE_API_ONLY=1` retains ordinary command reuse and installs no GPU
timestamp queries. Its `present-api` and `frame-api` lines isolate host API waits
without changing the GPU command stream. `DC_FRAME_TIMINGS=1` adds application
stages, including brush transactions outside presentation. Diagnose interaction
costs with controls enabled; idle replay alone does not establish brush latency.
`DC_FRAME_TRACE_FIXED_SECONDS=0.0166666667` makes the diagnostic executable use
a repeatable simulation clock for comparisons. This is necessary when comparing
shader variants: the ordinary app schedules additional fluid work as elapsed
frame time increases. Fixed-clock results are diagnostic only; final frame-budget
acceptance must use the ordinary application and its real elapsed simulation time.
For a reproducible displayed cutting workload, set
`DC_BENCHMARK_AIR_BRUSH=x,y,radius` on a bounded application benchmark. It applies
the real air brush at the given visible cell on every measured frame (starting
after the eight warm-up frames), with normal controls still active. Use a fresh
copy of the saved world and select a position intersecting actual geometry.

The normal quarter-native demo uses 480×270 simulation and rendered pixels,
upscaled four times for a 1920×1080 window. Its 640×448 active simulation region
includes chunk rounding and an offscreen neighbor halo, not supersampling.

Empty worlds start with an 8192-word pixel-geometry bank. Geometry and appearance
storage grow from GPU capacity requests; this initial granule is not a body or
shape limit. Terrain connectivity scratch remains sized for all resident cells
independently of the geometry bank. `DC_TEST_INITIAL_ARENA_ONLY=1` and
`DC_TEST_GEOMETRY_GROWTH_ONLY=1` on `dynamic_runtime_tests` check actual appearance
allocation and automatic growth for a 97,028-pixel component respectively.
