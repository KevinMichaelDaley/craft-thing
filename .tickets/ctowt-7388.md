---
id: ctowt-7388
status: in_progress
deps: [ctowt-o11u, ctowp-v2rf]
links: []
created: 2026-11-27T06:30:47Z
type: task
priority: 1
assignee: kmd
parent: ctopp-2c14
tags: [offscreen, streaming, physics]
---
# Apply the full water sweep through the paired owner transaction

## Design

Bind both current generations at the start of the same transport interval. Validate and atomically apply the native paired water write set and carried fields with an explicit receipt and durable recovery. An owner participates once in a transport interval, including domains touching more than one seam.

## Acceptance Criteria

The live split and monolithic water basins conserve stock and the carried state across repeated camera admission. A stale generation, rejected batch, or missing receipt cannot publish a partial write set or advance only one owner.

