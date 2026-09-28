---
id: dun-wwcl
status: open
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

