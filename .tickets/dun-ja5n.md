---
id: dun-ja5n
status: closed
deps: [dun-wc20]
links: []
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, granular]
---
# Sift falling dirt, sand, and gravel mixtures

Mixed falling grains segregate by size and density through the particle solver.

## Design

Parameterize grain radius, density, friction, and contact response per material in the MPM state; allow small grains to percolate through larger voids without a special screen-space swap rule. Use bounded local work per particle.

## Acceptance Criteria

A mixed-column interactive drop produces reproducible size/density sorting, stable piles, no particles crossing solid walls, and conserved counts/mass across chunk seams and reload.


## Notes

**2026-09-27T16:06:15Z**

GPU 3x3 pair contacts and kinetic sieving use material radii, density, and friction; mixed 63-grain pile sorts while preserving mass and walls through 160 ticks. UI sifting demo and one-second screenshot pass. Version 5 chunks persist radii; v4 loads migrate. make test, make test_ui, and Vulkan validation pass.
