---
id: ctowpe-k8de
status: in_progress
deps: []
links: []
created: 2026-11-27T14:16:32Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-t43r
tags: [offscreen, save, camera]
---
# Restore parked room ownership and catch-up after closing the level

## Design

Persist the coordinates, owner tokens and confirmed source epochs needed to prefetch sleeping sealed rooms after process restart. Couple the catalog to the same committed save as its chunk receipts, including stale-generation and retry handling.

## Acceptance Criteria

Evict several active rooms, park them outside four screen widths, advance time, close and reopen, then return. Each room must catch up exactly once from its saved epoch with its conserved stock and complete state. Reject a stale owner and recover an interrupted staged write.
