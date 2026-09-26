---
id: dun-igec
status: open
deps: [dun-xz0t]
links: []
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, fluid, erosion, streaming]
---
# Persist slow stone weathering in a separate GPU map

High-velocity water wears stone quickly relative to baseline flow, while ordinary flow erodes stone over simulated years.

## Design

Add a separate chunked fixed-point wear map and accelerated test clock. Accumulate bounded wear from water velocity/shear and elapsed simulation time on GPU, then convert thresholded stone mass into sediment/gravel with explicit accounting. Persist wear independently of visible cell material and avoid per-frame CPU readback.

## Acceptance Criteria

High-speed water and multi-year normal flow eventually release stone; short low-flow runs leave it intact; wear survives chunk eviction, and accelerated-time tests produce the same result as equivalent fixed ticks.

