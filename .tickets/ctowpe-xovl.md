---
id: ctowpe-xovl
status: in_progress
deps: []
links: []
created: 2026-11-27T19:14:10Z
type: bug
priority: 2
assignee: kmd
parent: ctowpe-t43r
---
# Compare camera marker records independently of GPU append order

Parked-room and paired-water return checks compare marker arrays by raw append order. Markers are emitted through atomic counters. The parked-room case also omits the marker records in the published disk snapshot.

## Acceptance Criteria

Reordered records compare equal while changes to IDs, kind, position, duplicates or count fail. Compare all marker records on both disk and resident returns.
