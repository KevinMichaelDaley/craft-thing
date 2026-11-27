---
id: ctowpe-t43r
status: closed
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

**2026-11-27T21:18:50Z**

Production camera/structure aggregate at f90a75ca on Xephyr :93: 45 passed, 0 failed. Covered 17 saturated rooms returning, opposite edges, parking and prefetch, immediate reversal, durable reopen, regular and irregular native replay, and complete water/particle/marker state at the confirmed time. All six separately measured production camera bands passed their mean and p95 frame budget, with 16 active owners and 2.000000104 seconds advanced per band (ctowpe-vtlh).
