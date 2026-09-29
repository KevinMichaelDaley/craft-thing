---
id: dun-4w8r
status: closed
deps: []
links: [dun-z5fb]
created: 2026-09-29T03:33:02Z
type: task
priority: 1
assignee: kmd
---
# Drive cold chunk activation from GPU wet frontier in both axes

## Design

Current dun-z5fb fix prefetches the last painted water row horizontally into four lazily created workspaces. Replace that heuristic with GPU activity metadata and a budgeted sparse workspace scheduler; do not copy full chunks per frame.

## Acceptance Criteria

Fluid crossing any side of a resident workspace activates the next cold chunk at bounded readback cadence or via GPU residency metadata; support multiple active water elevations; continue toward equilibrium across the configured offscreen physics band without the paint-history row or four-workspace limit.


## Notes

**2026-09-29T04:10:55Z**

Implemented Vulkan wet-edge mask dispatch with one 32-bit mask per GPU slot, sampled every four physics ticks. Cold neighbors stream on all four sides; 16 compact 4x4 GPU workspaces are created lazily. End-to-end tests cover two water elevations, falling into a cold lower chunk, far right propagation, and five simultaneous distant wet regions. make test test_ui test_half_native pass; half-native smoke measured 62.79 ticks/s.
