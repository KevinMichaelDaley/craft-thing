---
id: ctlo-mblk
status: in_progress
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
