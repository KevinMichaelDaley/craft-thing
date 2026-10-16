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
