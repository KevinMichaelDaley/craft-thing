---
id: dun-8qq2
status: closed
deps: []
links: []
created: 2026-09-27T22:38:29Z
type: feature
priority: 0
assignee: kmd
parent: dun-9e5b
---
# Interpolate staged Eulerian fluid and use elapsed-time timesteps

## Acceptance Criteria

Visible fluid state interpolates on GPU between six-tick updates; fluid displacement and source rate depend on elapsed simulation time rather than presentation rate; tests compare equivalent elapsed time under different frame cadences without per-frame readback.


## Notes

**2026-09-27T23:14:01Z**

Elapsed time schedules a fixed six-phase Eulerian update every 0.1 simulated seconds. 60 Hz and 30 Hz runs over equal wall time match mean falling-water depth exactly (26.02 cells); GPU snapshot blending renders between fluid updates. Native adaptive smoke simulated 0.287 s in 0.305 s wall over 12 frames.
