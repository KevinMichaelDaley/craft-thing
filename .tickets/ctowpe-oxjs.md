---
id: ctowpe-oxjs
status: closed
deps: []
links: []
created: 2026-11-27T11:58:04Z
type: task
priority: 2
assignee: kmd
parent: ctopo-yhkb
tags: [streaming, physics]
---
# Resolve rigid contacts with granular particles owned by a neighboring liquid tile

## Design

Paired rigid terrain queries can see the neighboring page and its fixed material. Granular contacts also need the particle pool, scratch and solver references for the owning tile; those references currently resolve only locally.

## Acceptance Criteria

A moving body interacting with loose material across an owner face matches monolithic particle and rigid momentum, contact geometry and stock. Preserve the normal absent-owner boundary and validate under Vulkan synchronization checking.


## Notes

**2026-11-27T13:25:17Z**

RTX A2000 on persistent hidden Xephyr :93: both owner submission orders match native world contact records, rigid state, grain identity, stock and complete particle records after the MPM step. The missing-owner boundary and nonzero horizontal impulse are asserted. Both passed with Vulkan synchronization validation. Related ordered gather, packed copy, swap/reload, raster and support-graph tests also passed.
