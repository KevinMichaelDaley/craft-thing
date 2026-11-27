---
id: ctowtb-kz7x
status: open
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
