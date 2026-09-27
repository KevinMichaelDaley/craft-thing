---
id: dun-tw2z
status: closed
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


## Notes

**2026-09-27T22:06:07Z**

Native build now presents 1920x1080 at 1 simulated pixel per display pixel with 608 resident 64x64 chunks covering 30x17 visible chunk tiles plus one-chunk halo. Fullscreen interactive path, streamed generation, far-edge brush test, and native screenshot smoke pass. Twelve presented physics ticks averaged 8.50 Hz on RTX A2000 with six-stage fluid update.
