---
id: ctowd-09br
status: closed
deps: [ctowtb-kz7x]
links: []
created: 2026-11-27T07:42:43Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Solve a connected set of water owners in one native cycle

## Design

Gather the connected mapped tiles of the resident context and a shared-device workspace into one model of their world-space domain. Solve each tile once and publish the entire set under one retained receipt. Preserve native sealed boundaries and slot-local marker coordinates.

## Acceptance Criteria

Compare a three-tile basin with one tile touching two seams against a monolithic cycle. Verify material stock, velocity, markers, pigment and heat. A stale mapping or failed publication validation leaves all three owners unchanged.

## Notes

**2026-11-27T08:12:55Z**

The three-tile basin matches the monolithic water solver with specialized and generic pipelines under Vulkan synchronization validation. The fixture covers two seams, partial apertures, mixed marker banks, stale and duplicate mappings, validation failure, receipt recovery, and an excluded mapped tile. Both axis tests for the original pair also passed with both pipeline configurations. 81 host tests, 54 GPU tooling tests and seven SPIR-V validations passed. The remaining native regression and sanitizer runs are queued for shared GPU resources.
