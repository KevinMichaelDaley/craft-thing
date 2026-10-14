---
id: ctpf-753m
status: closed
deps: []
links: []
created: 2026-10-14T03:55:22Z
type: task
priority: 1
parent: ctpf-rvcy
tags: [physics, structural, fracture, authoring]
---
# Audit remaining feature gaps and agree atomic authoring/provenance ABI

Separate existing mechanics and optimizer rewrites from missing new authoring/release behavior. Agree canonical maps, world origin/pivot/transaction IDs, reservations and appearance/provenance hooks with extraction/renderer/exporter owners.

## Acceptance Criteria

Concrete capability/acceptance gap matrix and minimal API contract, compiling RED tests for actual unsupported inputs, bounded GPU design and kernel ownership boundaries. No invented unrelated advanced physics or duplicate graph solve.


## Notes

**2026-10-14T04:10:14Z**

Capability audit and admission ABI agreed with optimizer and renderer. Six bounded CPU envelope checks pass; sparse Body/shape ABI unchanged. Explicit graph/member ownership is required beyond the current exporter role hints. See design/authored_physics_gap_audit.md and include/ferrum/structural/authoring.h. Full GPU regression remains queued; no placement completion claimed.
