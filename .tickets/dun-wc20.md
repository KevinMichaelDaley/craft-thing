---
id: dun-wc20
status: in_progress
deps: [dun-hxhe]
links: [dun-u68k]
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, materials]
---
# Implement GPU P2G, grid solve, and G2P for granular materials

Move dirt, sand, and gravel with a particle/grid MPM stage after the rigid and Eulerian water stages.

## Design

Use bounded particle-to-grid and grid-to-particle compute passes, a stated stress model and stability/substep bound, solid/rigid boundary coupling, and cross-chunk halo exchange. Resolve simultaneous writes with a deterministic or documented repeatable reduction scheme and track fixed-point mass.

## Acceptance Criteria

A pile settles without particle loss or explosions; falling mixtures cross chunk seams and collide with stone and bodies; headless replay and an interactive paint/fall scene pass with Vulkan validation and measured GPU timings.


## Notes

**2026-09-26T23:38:13Z**

Storage now has 4096 fixed primary cell slots plus 4096 reserved records per chunk. Implement deterministic allocation/compaction for reserved records, GPU MPM P2G/grid/G2P, and GPU timestamp reporting in this ticket. Current storage tests report synchronous wall times only.
