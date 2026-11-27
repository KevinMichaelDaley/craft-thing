---
id: ctowpe-t43r
status: in_progress
deps: [ctowpe-k8de, ctowpe-l2cj, ctowpe-vtlh]
links: []
created: 2026-11-27T14:01:15Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, camera]
---
# Retain and prefetch multiple sleeping camera workspaces on the shared world clock

## Design

Port the bounded sleeping cache and distance bands from 7347531 onto the current confirmed owner and staged-save lifecycle. Preserve the full native cadence for connected resident seams. Schedule sealed near, middle and far groups by elapsed time and release distant groups through confirmed worker writes.

## Acceptance Criteria

A production camera regression demotes multiple active sealed rooms, including opposite edges and a saturated cache. Every owner is advanced or durably parked without duplicate work. Far rooms sleep beyond four view widths and prefetch on approach. Returned rooms retain their complete physical state and confirmed time. Measure native and half-native frame costs separately from setup.


## Notes

**2026-11-27T14:21:50Z**

Five focused Xephyr camera cases passed with synchronization validation: simultaneous evictions, opposite edges, release beyond four view widths and prefetch on approach, all 17 saturated rooms caught up with exact individual water stocks, and completion of a due middle-band interval. The 22 level-tick checks also passed. Remaining work includes reopening the dormant catalog and measuring half-native and native presented frame costs.
