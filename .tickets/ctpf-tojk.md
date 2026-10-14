---
id: ctpf-tojk
status: open
deps: []
links: []
created: 2026-10-14T04:19:39Z
type: task
priority: 1
parent: ctpf-rvcy
tags: [physics, structural, prop, exporter]
---
# Produce validated companion structural graph and member plane for prop assets

Add explicit authored companion mechanical graph and aligned member-ID ownership plane for LibreSprite prop packages. Existing role/object/anchor planes cannot define beam topology, section properties or joints. Coordinate exporter runtime integration ctpe-847v and final acceptance ctpe-g0sl without editing the completed sibling exporter in this feature worktree.

## Acceptance Criteria

Versioned companion schema, byte lengths and hashes validated before staging; stable graph IDs/generations and member pixel ownership agree with real source geometry/material properties; anchor hints require actual terrain contact; missing/corrupt/unsupported metadata diagnosed atomically. No inferred rectangular proxies or synthetic support. Real artist-authored package exercised through GPU placement, release and reload.


## Notes

**2026-10-14T04:21:24Z**

Producer schema proposal: design/prop_structural_companion_v1.md. Parent will resume exporter worker for producer integration once a slot frees; feature worker only consumes metadata. Existing ctpe-847v/ctpe-g0sl structural placement must remain gated on a real validated companion producer.
