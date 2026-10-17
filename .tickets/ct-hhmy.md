---
id: ct-hhmy
status: open
deps: []
links: [ct-yd8t, ct-wuci]
created: 2026-10-17T07:50:24Z
type: epic
priority: 1
tags: [vulkan, rigid, dynamic-capacity, parallel]
---
# Grow rigid body storage and GPU work dynamically without a fixed body-count limit

Explicit user requirement October17: simulation must support a dynamic number of rigid bodies, not a larger fixed64 constant. Exact saved sandbox filled all64 active owned records, leaving newly detached stone as static gray terrain. Preserve source, geometry, IDs, mass and checkpoints across growth; no deleting bodies or CPU physics fallback.

## Acceptance Criteria

Grow body records, ownership metadata, scratch/work queues and associated resources as required. GPU-parallel reservations/counts/indirect dispatches use actual live work; host only manages Vulkan allocations/descriptors/lifetimes at safe boundaries. Support repeated growth beyond64, mixed types, offscreen known components, birth/fracture bursts, reuse, allocation failure rollback/backpressure and retry, checkpoint migration. Capacity independent of structural node/member limits. No compile-time body-count ceiling, arbitrary bigger max, ordinary-frame CPU physics solve/readback fallback, source loss or performance gate relaxation.

