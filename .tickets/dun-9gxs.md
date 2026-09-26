---
id: dun-9gxs
status: open
deps: [dun-wc20, dun-3bl8, dun-s8rr]
links: [dun-2nbs]
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, fluid, mud]
---
# Couple Eulerian water to dirt moisture and mud

Small water additions wet dirt into mud while retaining both solid and water mass.

## Design

Transfer a bounded, exact water amount from Eulerian cells into particle moisture and back on drying; moisture changes MPM cohesion/yield parameters with explicit wet/dry thresholds and hysteresis. Avoid double-counting liquid in render or flux.

## Acceptance Criteria

A small dose changes dirt to deformable mud; dry and saturated controls behave differently; water plus bound moisture is conserved through motion, seams, and streaming, with no CPU physics readback.

