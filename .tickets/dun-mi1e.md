---
id: dun-mi1e
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, rigid]
---
# GPU rigid-body simulation stage

Integrate bodies on GPU, rasterize occupancy, solve terrain contacts, and publish final body occupancy before the fluid pass.

## Acceptance Criteria

A body falls and settles on terrain, reacts to a terrain edit on the next tick, cannot tunnel under the documented speed/substep bounds, and exposes stage diagnostics.

