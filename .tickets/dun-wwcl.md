---
id: dun-wwcl
status: closed
deps: []
links: []
created: 2026-09-28T02:43:03Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Build bounded GPU cache for offscreen chunk physics

Keep nearby dynamic world chunks resident across viewport eviction using a bounded GPU cache/workspace, with velocity, markers, particles and mass owned by world chunk. Stream cold chunks on the worker; do not read back every frame.

## Acceptance Criteria

One- and two-screen-offscreen chunks keep GPU state and can be promoted without reset; cache size is bounded and benchmarked at half/native resolution; camera test passes without new per-frame copyback.


## Notes

**2026-09-28T03:05:22Z**

GPU cache now keeps departed dynamic chunks active at 4/12/24-tick cadence with water and particle state, then flushes beyond four screens. One- and two-screen water pan tests and sand particle-ID test pass. Remaining blocker: conservative flux at visible/offscreen and workspace boundaries (dun-bd93); fixed context pool can saturate. Full half/native benchmarking still needed.

**2026-09-28T03:07:47Z**

Interactive smoke caught a restart persistence regression: offscreen-promoted chunks were marked clean and could reopen from stale disk velocity. Promotion now marks chunk dirty; --smoke-stream passes including face-velocity round trip.

**2026-09-28T03:13:53Z**

Smoke benchmark on current GPU: half-native 960x540 60 ticks in 3.597 s (16.68/s), 164.4 MiB mapped system + 286.4 MiB device VRAM per foreground context; native 1920x1080 60 ticks in 6.331 s (9.48/s), 534.4 MiB mapped system + 931.1 MiB device VRAM. Adaptive presentation 2.5 and 1.9 FPS. Full offscreen ring will need tighter sparse allocation and scheduling.
