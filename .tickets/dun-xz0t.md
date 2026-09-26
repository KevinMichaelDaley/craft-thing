---
id: dun-xz0t
status: open
deps: [dun-9gxs]
links: []
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, fluid, erosion]
---
# Erode dirt under strong water flow

High-volume or high-shear Eulerian water detaches dirt/mud into mobile sediment particles.

## Design

Sample projected face velocity and water depth at solid boundaries; use a bounded GPU erosion rate and sediment carrying capacity. Debit stationary dirt and credit mobile sediment exactly once. Deposition reverses the transfer when flow weakens.

## Acceptance Criteria

A repeatable high-flow scene cuts a dirt bank while a trickle does not; detached mass appears downstream or deposits, total solid mass is conserved, and chunk-edge results match an unpartitioned scene.

