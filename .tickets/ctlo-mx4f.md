---
id: ctlo-mx4f
status: closed
deps: []
links: []
created: 2026-10-24T08:23:59Z
type: task
priority: 1
---
# Specialize automatic rigid solving when no explicit structural graph exists

## Acceptance Criteria

Keep impact fracture and all eight-by-eight contact iterations, but omit empty bond dispatches and structural-load shader branches for worlds without explicit members/bonds. Conservative host-owned graph-presence tracking must activate on authoring and snapshot restoration; command-cache keys must track transitions. Compare exact rigid states and live/restored structural reactions, run graphical full-frame measurement, preserve dynamic body capacity.


## Notes

**2026-10-24T08:31:47Z**

RED confirmed 80 empty bond dispatches. GREEN recording tests 5/5 and cache tests 8/8; explicit graphs retain all 80 passes and authoring invalidates cached commands. Actual Vulkan core/synchronization validation comparison passes with exact full rigid records after dynamic queue growth; structural load queries 4560 to zero. Real displayed 500-frame brush measurement running; frame gate not yet established.

**2026-11-28T06:00:55Z**

Current actual Vulkan tests passed all five solver-recording checks, all ten command-cache checks and the dynamic-growth exact-body comparison. The last reported 4560 structural-load reads with explicit graph evaluation and zero on the automatic path. Displayed two-anchor bridges (centered and eccentric loads) reproduced every reaction component byte after fresh-context restoration and one resumed tick, and changed zero pixels. Whole displayed brush frames were measured at 4.613 to 5.290 ms mean across the three 90-frame windows in a passing run. The documented original Intel saved-level benchmark is in design/inline_frame_optimization.md. Recreated-window startup reliability and the current native360 frame budget remain tracked independently.
