---
id: ctowpe-oxjs
status: in_progress
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

