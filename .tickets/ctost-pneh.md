---
id: ctost-pneh
status: closed
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

## Notes

**2026-11-27T02:50:47Z**

Stream reservations now roll back on enqueue rejection and worker failure. Each save has a fresh operation generation. Camera and pinned-transfer regressions cover partial remap, stale completions, destination rebind, and preservation of prior saved data and elapsed time. Verified 49 targeted host checks, 22 ASan/UBSan checks, and five RTX A2000 Vulkan checks with validation. All 143 compiled make test prerequisites built. The complete make test execution is queued behind the earlier GPU suites. Coherent DCS/DCC publication continues under ctost-uuni.
