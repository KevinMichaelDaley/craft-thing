---
id: ctlo-mblk
status: closed
deps: []
links: []
created: 2026-10-24T08:47:21Z
type: task
priority: 1
---
# Expose immutable runtime root and geometry to contact generation

## Acceptance Criteria

Contacts compile with NonWritable Restrict runtime root, directory and geometry, while contact scratch stays writable. RED compiled contract then GREEN; all contact shapes, dynamic queue growth and concave visual dynamics pass synchronization checks. Measure displayed frame budget.


## Notes

**2026-10-24T08:52:53Z**

RED compiled contract has3 failures plus absent immutable geometry block. GREEN compiled contract5/5; all10 contact tests pass actual core/synchronization validation, including rotated shapes, powders/both layers, rebasing, symmetry, overflow and missing chunks. Displayed benchmark running.

**2026-11-28T05:02:46Z**

Production contact suite passed 10/10 with core/sync validation. The same validation passed all five dynamic runtime fixtures, including exhausting and growing both pair/contact queues then retrying without lost bodies. The displayed concave-body fixture also passed 180 ticks in 3.245s with exact masses and stable paused captures. Actual frame-budget measurement on this RTX produced 15.993ms mean and 62.538ms worst at 480x270 under concurrent load. The independent strict frame-budget ticket remains open.
