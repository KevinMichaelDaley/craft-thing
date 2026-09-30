---
id: dun-sprn
status: closed
deps: []
links: []
created: 2026-09-30T03:42:15Z
type: task
priority: 0
assignee: kmd
parent: dun-v2vu
tags: [gpu, performance, ui]
---
# Run the interactive world at quarter-native simulation resolution

Replace the half-native interactive target with a 480x270 simulated view upscaled to 1920x1080, preserving world scale and interactivity. Retain half-native as benchmark/reference.

## Acceptance Criteria

Quarter-native config, build and smoke tests pass; 60-frame representative benchmark records frame, fluid and granular GPU times; interactive demo opens at 1920x1080 and painting/camera input work; full make test passes.


## Notes

**2026-09-30T03:47:57Z**

Quarter-native 480x270 / 70 chunks / fluid every physics tick. 60-frame smoke: 61.3 ticks/s, adaptive 60.7 FPS, fluid GPU 4.5 ms/tick (5.3 peak), granular GPU 1.3 ms/tick, device-local buffers 107.2 MiB. Config and mud smoke pass; full make test passes. Upscaled screenshot inspected.
