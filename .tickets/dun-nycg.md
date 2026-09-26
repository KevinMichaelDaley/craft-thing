---
id: dun-nycg
status: in_progress
deps: [dun-zbeg, dun-f6oh]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
tags: [ui, level]
---
# Add interactive material brush and camera controls

Provide material selection, paint/erase brush, pan/zoom, pause, single-step, reset, seed input, and visible chunk loading state.

## Acceptance Criteria

Brush edits affect correct world cells across negative and positive chunk coordinates; pausing and stepping advance exactly one fixed tick; camera crosses chunk boundaries smoothly.


## Notes

**2026-09-26T06:09:14Z**

Interactive testbed now has P pause/resume, N one fixed simulation tick, B spawn rigid box at pointer, and 60 Hz accumulator with catch-up cap. Remaining acceptance includes zoom, reset, visible loading/debug state, and smooth camera movement.

**2026-09-26T23:08:50Z**

Pixel camera moves within the one-chunk simulation halo and rebases GPU velocity only at 64-cell crossings. GUI holds WASD/arrows for cell pan; title reports 24-chunk load progress. R and F2 create a fresh seeded run in a unique directory and preserve the previous saved world. SDL-event smoke exercises seed and reset.

**2026-09-26T23:11:22Z**

Final validation: make test test_ui test_ui_long passed; Vulkan validation layer passed controls and scripted SDL input. Exact 1-step test and rendered camera crop tests cover negative X/Y seam crossings.
