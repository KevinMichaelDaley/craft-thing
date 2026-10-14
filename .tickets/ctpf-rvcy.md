---
id: ctpf-rvcy
status: open
deps: []
links: []
created: 2026-10-14T03:55:22Z
type: epic
priority: 1
tags: [physics, structural, fracture, authoring]
---
# Implement remaining authored structural and fracture behavior separately from optimization

User explicitly split ct-wuci: existing agent owns kernel parallelization/performance; this isolated feature worker implements only demonstrably missing NEW fracture/structural behavior and atomic multi-role authoring for props. See design/physics_feature_split.md. Base6e723cd; preserve optimizer's existing birth/hydro/contact/raster kernels.

## Acceptance Criteria

Audit proven behavior gaps; exact arbitrary mixed-material props and structural graphs authorable atomically; release/fracture ownership and conservation correct; renderer/source-texel provenance and persistence preserved. GPU-parallel runtime, TDD, actual visuals and combined validation before main integration. No reimplementation of already-complete structural mechanics.

