---
id: dun-9gxs
status: closed
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


## Notes

**2026-09-27T01:22:19Z**

GPU cell-owner pass binds/release exact 16.16 water mass to dirt particle flags, with 2048/1024 hysteresis, 16384 cap, 4096/tick uptake, 64/tick release. Mud uses softer MPM bulk/shear/yield. Water painting retains granular particles; granular painting retains free water. Tests cover small dose, drying return, dry/saturated mechanics, exact free+bound mass across a seam and stream save/reload, GPU stage times, Vulkan validation, and windowed 1s combined material screenshots (201 changed pixels). make test and make test_ui passed.
