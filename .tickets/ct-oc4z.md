---
id: ct-oc4z
status: in_progress
deps: []
links: [ctlo-inhv]
created: 2026-11-01T18:33:33Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, performance, catapult]
---
# Profile and reduce RTX catapult scene startup before first rendered frame

On 2026-11-01, the real DISPLAY=:1 NVIDIA RTX A2000 catapult_demo created a 1024x512 desktop window but took roughly two minutes at about one CPU core before reporting game view ready. Focused catapult scene and Xephyr display tests similarly spend tens of seconds before first frame. The user expects a visible, responsive demo on launch.

## Design

Instrument GPU creation, pipeline compilation, scene upload, structural placement, combustion setup, renderer enable, and first submit independently; retain timing traces in a checked-in diagnostic. Use cached or deferred initialization where supported without changing physics or output.

## Acceptance Criteria

A cold and warm catapult_demo on the actual RTX display render the first frame quickly, with measured setup phases and no blank desktop window for tens of seconds. Keep the 256x128 game resolution and full launch/ignition behavior; focused tests and full make test pass.


## Notes

**2026-11-27T00:32:27Z**

The catapult executable now has a first-frame startup probe on its actual 256x128 load, fixed launch scene and lit presentation path. Run make test_catapult_startup with an X11 Vulkan-capable display, or run ./build/catapult_demo --startup-probe directly. DC_BONFIRE_STARTUP_TRACE=1 also exposes the existing device, shader-pipeline and swapchain milestones. Use DC_PIPELINE_CACHE_DIR and DC_PIPELINE_CACHE_TRACE=1 to compare a fresh and then warmed pipeline cache. The portable regression reproduces and verifies the launch marker on an unavailable display. RTX cold and warm first-present evidence and startup reduction remain open.

**2026-11-27T00:42:31Z**

Pinned 255d0c96 completed both first-present cases on the RTX A2000 with the isolated X11 display. Empty Vulkan cache loaded 0 bytes and took 181.437 s to first present, including 102.065 s GPU setup and 48.448 s thermal setup. The detailed cold GPU milestones were structural pipeline 78.213 s and tick pipeline 101.648 s cumulative. Reusing the 29,892,603-byte cache took 9.533 s. Warm gas setup was 3.181 s, structural settle 2.515 s, and renderer setup 1.903 s. Both runs presented one lit 256x128 frame and passed 2/2 checks. Other GPU jobs were active. The measured startup performance ticket remains open.

**2026-11-28T04:19:36Z**

RTX A2000, hidden Xephyr, 256x128, f2a02ab6: cold cache loaded zero bytes and presented the lit frame after 156.046 s, including GPU setup 84.340 s and thermal setup 44.866 s. Warm loaded 28118997 bytes and presented in 3.769 s. Both probes passed 2/2. Another workspace used the GPU during the measurement, so the difference from the October/November baseline is descriptive. Cold start still misses the responsiveness requirement.

**2026-11-28T05:16:02Z**

After offline thermal phase specialization, the isolated displayed RTX A2000 first-present probe passed both cases cold and warm. Cold first present took 115.321 s (general GPU setup 78.353 s, thermal 8.729 s). Warm took 2.461 s (GPU 1.265 s, thermal 0.031 s). The previous cold thermal interval was 44.866 s. Cold general physics pipeline creation still dominates and fails the quick-launch criterion.
