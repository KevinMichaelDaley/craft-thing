---
id: ctost-pneh
status: in_progress
deps: []
links: []
created: 2026-11-27T02:38:02Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, world-clock]
---
# Make level stream handoffs recover from failed worker operations

Camera and transfer routes reserve generation-owned slots before queuing disk work. A rejected enqueue or failed worker completion currently leaves the slots loading or saving indefinitely, and repeated writes share a residency generation.

## Design

Use distinct write-attempt generations while retaining residency identity. Release a failed matching load reservation and restore a failed matching write to its existing dirty owner. Preserve pinned transfers and ignore all stale replies.

## Acceptance Criteria

Level regressions force rejected load and save queues and disk failures, then verify successful retry from the same gameplay path without stale publication, duplicated work, or lost stock and completed time.
