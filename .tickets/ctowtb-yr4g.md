---
id: ctowtb-yr4g
status: in_progress
deps: [ctowp-v2rf]
links: []
created: 2026-11-27T07:11:47Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Finish native water correction inside the paired publication

## Design

Gather the two active marker banks and cell seeds into the paired solver. Run the native mass correction, marker advection and velocity damping after advection, then publish the corrected fields and markers under the existing batch receipt. Keep advection pressure work separate from marker-driven exchanges.

## Acceptance Criteria

Compare marker-enabled split basins with the complete monolithic water cycle, including different active marker banks, both axes, conserved carried state, unchanged owners after rejection and publish/receipt recovery.
