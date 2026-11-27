---
id: ctowd-09br
status: open
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
