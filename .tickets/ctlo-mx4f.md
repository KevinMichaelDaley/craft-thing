---
id: ctlo-mx4f
status: in_progress
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
