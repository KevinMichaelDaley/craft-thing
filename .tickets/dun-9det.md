---
id: dun-9det
status: open
deps: []
links: [dun-p9g9, dun-9e5b]
created: 2026-09-26T07:42:49Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, mpm, materials, fluid]
---
# GPU MPM granular materials and water-driven geology

Dirt, sand, and gravel use a GPU particle/grid MPM solver coupled conservatively to Eulerian water. Moisture creates mud; strong flow erodes dirt; persistent slow wear erodes stone; falling mixtures sift by grain size and density.

## Design

Use world-coordinate particles in chunk-owned bounded pools with GPU P2G/grid solve/G2P and a sorted or deterministic conflict policy. Keep Eulerian water authoritative; exchange water and solid mass only through budgeted equal-and-opposite GPU transactions. Slow geology uses a separate per-cell wear/moisture-age map persisted with chunks. Rigid stone conversion and fracture are separate dependent tasks.

## Acceptance Criteria

All movement and reactions run in Vulkan compute/SPIR-V without normal-frame device-to-host readback. Particle counts and mass budgets survive chunk crossings and worker save/reload. A playable mixed scene and GPU stage timings accompany each increment.

