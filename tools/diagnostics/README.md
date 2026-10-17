# Bounded Vulkan device probe

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

`DC_CONTACT_PAIR_PROFILE=1` additionally freezes displayed frame 72 and measures
each existing broadphase candidate separately. This explicit diagnostic copies
and restores all touched body storage and backs up/restores device-only activity
storage on the GPU. It never runs a CPU physics solve. Candidate count is bounded
to 64, and the command still requires its external deadline. This extra work
perturbs frame timing; use its per-candidate results for attribution, not FPS
acceptance. Run with synchronization validation before using those timings.
