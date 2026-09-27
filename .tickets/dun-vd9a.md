---
id: dun-vd9a
status: in_progress
deps: []
links: [dun-tw2z]
created: 2026-09-27T21:41:36Z
type: task
priority: 0
assignee: kmd
parent: dun-9e5b
---
# Reach 60 Hz fluid ticks for fully resident native view

## Acceptance Criteria

With 1920x1080 fully resident chunks and representative water and grains on RTX A2000, sustain 60 Hz GPU physics without copyback; profile hydrostatic scans, transport, marker corrections, pressure convergence and memory traffic; preserve fluid correctness.


## Notes

**2026-09-27T22:06:07Z**

After full 608-chunk residency and six-stage fluid update, native 1920x1080 runs 12 presented physics ticks in 1.412 s (8.50 Hz), 76-138 ms frame range on RTX A2000. Continue performance work toward 60 Hz.

**2026-09-27T23:14:01Z**

Native 1920x1080/608 chunks improved from 8.5 to about 39-43 presented ticks/s. GPU average stage ms/tick: rigid 0.9, fluid 9.5, granular 7.2; peak fluid phase ~14 ms after redistribution. 60 Hz target remains open; active sim buffers now device-local and fluid runs at wall-clock speed via variable phase count.
