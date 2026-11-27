---
id: ctoae-hgsd
status: closed
deps: [ctdot-kbys]
links: []
created: 2026-11-27T04:25:06Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, physics]
---
# Keep empty terrain from taking the offscreen simulation slot

Resident time advances mark all mapped slots dirty. One sealed solid tile can win the only background workspace even when it has no transport stock, leaving an active sealed tile on disk.

## Design

Check the immutable eviction snapshot for live water, bulk-liquid or gas stock before electing it. Continue committing empty terrain through the ordinary durable worker route.

## Acceptance Criteria

A sealed empty tile leaves the slot available, then a sealed oil-only tile can claim the same slot. Preserve the existing two-screen exact-stock, retry and cold-reopen behavior.

## Notes

**2026-11-27T04:29:47Z**

The host regression reproduced the empty-tile election, then passed after eligibility was limited to saved water, bulk liquid or gas stock. It confirms an empty sealed tile leaves the route available and an oil-only tile can claim it. All 80 focused host cases and both native camera cases passed, with core and synchronization validation enabled. The 27 native streaming, workspace and checkpoint cases also passed on the preceding camera revision.
