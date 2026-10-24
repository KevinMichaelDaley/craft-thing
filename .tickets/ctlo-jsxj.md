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

