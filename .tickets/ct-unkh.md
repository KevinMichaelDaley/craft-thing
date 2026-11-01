---
id: ct-unkh
status: open
deps: [ct-ortp]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Chew real tree material into independently owned wood inventory

Add extensible inventory/item interfaces and a chewing interaction against actual nearby tree wood. Choose and document an available provisional chew input; preserve requested A/D/Space/Q/E bindings.

## Acceptance Criteria

Chewing plays all 8 frames and consumes valid reachable tree wood from world atomically with wood credited to that player inventory. No wood duplication from retries, held input, multiple players or missing chunks. Reject out-of-range/non-tree/empty targets without rewards. Inventory is per-player and item definitions are extensible without assuming RPG rules. Trees visibly change; tests cover conservation and ownership.

