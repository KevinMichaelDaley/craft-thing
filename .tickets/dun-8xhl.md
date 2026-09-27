---
id: dun-8xhl
status: closed
deps: []
links: [dun-2b08, dun-tw2z, dun-z337]
created: 2026-09-27T21:37:27Z
type: task
priority: 0
assignee: kmd
parent: dun-9e5b
---
# Accelerate native-grid pressure projection

## Acceptance Criteria

Profile and reduce the 1920x1080 fluid stage from the measured 269 ms sparse-grid baseline using active-page dispatch or equivalent GPU work pruning while preserving mass and divergence tests; rerun native benchmark and report tick rate.


## Notes

**2026-09-27T21:41:40Z**

Native sparse-grid fluid GPU stage improved from 268.800 ms to 78.809 ms; tick rate from 3.62 to 11.72 Hz on RTX A2000 at 1920x1080 with 64 resident chunks. Pressure and per-cell fluid dispatches use GPU slot-to-page mapping. Fluid tests and Vulkan validation pass.
