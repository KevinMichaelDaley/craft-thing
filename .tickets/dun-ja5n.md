---
id: dun-ja5n
status: open
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

