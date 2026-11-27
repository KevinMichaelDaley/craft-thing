---
id: ctcos-3q4u
status: closed
deps: []
links: []
created: 2026-11-27T03:09:36Z
type: task
priority: 1
assignee: kmd
parent: ctost-uuni
tags: [offscreen, streaming, world-clock]
---
# Commit offscreen snapshot ownership before superseding pending structure terrain

The structure store can now reopen a complete physical owner with its own fluids. The production camera route still needs evidence that a completed offscreen workspace handoff superseded a particular structure owner before it can publish the DCC copy.

## Design

Carry a durable ownership receipt from the level and workspace through a generation-checked worker publication. Verify coordinates, the replaced owner, and snapshot correspondence. Retain the previous pending structure owner through interrupted writes and retire it only after the accepted replacement binds. Keep legacy fallback when no receipt exists.

## Acceptance Criteria

Exercise a matching workspace takeover, stale and wrong-coordinate acknowledgements, interruption before publication, and cold reopen with updated topology and conserved fluid, parcel and elapsed-time state.

## Notes

**2026-11-27T03:57:29Z**

DCS v12 stores stable identities for pending structure chunks. DCC v13 checksums takeovers with their exact predecessor. The level routes workspace loads and write acknowledgements by coordinate, operation, generation and owner. A failed write leaves the workspace and archive available for retry. The old structure packet is consumed after all replacement channels bind. Verified 55 host checks, 19 ASan/UBSan checks, and 25 Vulkan checks with core and synchronization validation. The native cases cover an actual advanced workspace, a rejected disk write, stale and cross-coordinate acknowledgements, duplicate parcel failure during bind followed by a valid retry, and cold reopen. All 627 build prerequisites are ready and make test is queued for this exact revision. Normal camera eviction integration remains under ctdot-kbys.
