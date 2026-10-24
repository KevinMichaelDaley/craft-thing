---
id: ctlo-jsxj
status: in_progress
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
