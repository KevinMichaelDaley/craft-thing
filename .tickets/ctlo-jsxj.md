---
id: ctlo-jsxj
status: closed
deps: []
links: []
created: 2026-10-24T06:40:28Z
type: task
priority: 1
---
# Declare nonaliasing MPM particle and scratch buffers for shader optimization

## Acceptance Criteria

Ordinary MPM runtime storage blocks carry Restrict decorations; packed commit aliases remain legal. Particle, grain-contact, color and support results remain unchanged. Measure real displayed frame time without reducing physics work.


## Notes

**2026-10-24T06:46:00Z**

Nonaliasing declarations pass shader contract and exact raster tests; 500 ordinary real-time Wayland frames remain above budget at35.054ms mean /43.134ms worst. No full-frame improvement claimed. Packed particle aliases are explicitly exempted.

**2026-11-28T04:09:47Z**

Rechecked compiled ordinary and runtime SPIR-V: the disjoint MPM blocks have Restrict, and neither packed-commit variant restricts its aliased particle pools. The full suite passed particle identity, grain contacts, support and color checks. The ticket records its ordinary 500-frame measurement (35.054 ms mean, 43.134 ms worst).
