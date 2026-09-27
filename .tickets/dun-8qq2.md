---
id: dun-8qq2
status: in_progress
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

