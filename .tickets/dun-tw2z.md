---
id: dun-tw2z
status: open
deps: []
links: [dun-8xhl, dun-2b08, dun-z337, dun-vd9a]
created: 2026-09-27T21:37:23Z
type: feature
priority: 0
assignee: kmd
parent: dun-v2vu
---
# Make native viewport fully resident and interactive

## Acceptance Criteria

1920x1080 SDL viewport at one simulated cell per display pixel; enough resident GPU chunk slots for viewport plus one-chunk halo; threaded streaming and brush/camera controls work; benchmark GPU simulation and presentation without per-frame copyback.

