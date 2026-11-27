---
id: ctowtb-kz7x
status: closed
deps: [ctowtb-yr4g]
links: []
created: 2026-11-27T07:29:59Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Include native rigid displacement in the paired water cycle

## Design

Run native lateral and upward displacement on the shared sealed domain before projection. Carry water pigment and heat, count displaced crossings separately from advected pressure work, and preserve stock that a fully enclosed source cannot displace.

## Acceptance Criteria

Compare both seam axes against a monolithic domain with occupied water sources. Verify cross-seam displacement, bounded retained stock in an enclosed cell, water pigment and heat conservation, and atomic publication. The advection work ledger excludes forced displacement.

## Notes

**2026-11-27T07:47:40Z**

Both sealed axes match the native displacement, projection, advection and correction sequence. The fixtures verify forced cross-seam stock, sealed retained stock, conserved water pigment and heat, pressure work from advection, an unoccupied pair, and publication without a diagnostic plan. Passed 28 targeted Vulkan cases with synchronization validation, 81 host cases, 54 GPU tooling cases, eight SPIR-V validations and both ASan/UBSan cases. Full make test is running from the same source revision.
