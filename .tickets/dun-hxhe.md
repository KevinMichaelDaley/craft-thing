---
id: dun-hxhe
status: in_progress
deps: [dun-f6oh, dun-d8gb]
links: []
created: 2026-09-26T07:43:17Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, streaming]
---
# Store GPU MPM particles in streamed world chunks

Represent dirt, sand, and gravel as bounded world-coordinate particles with stable IDs, material, grain size, mass, and deformation/velocity state.

## Design

Keep GPU-owned active pools and chunk page ownership; compact or sort deterministically at chunk boundaries. Save/reload through the existing worker only on eviction. Define explicit capacity overflow and activation policy. One material pixel initially maps to a bounded particle budget; no full-frame CPU copies.

## Acceptance Criteria

Painted and generated particles render at cell scale, cross chunk seams, survive eviction/reload with exact count and mass, and replay reproducibly. GPU capacity and per-stage timings are reported.

