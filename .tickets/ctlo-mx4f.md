---
id: ctlo-mx4f
status: open
deps: []
links: []
created: 2026-10-24T08:23:59Z
type: task
priority: 1
---
# Specialize automatic rigid solving when no explicit structural graph exists

## Acceptance Criteria

Keep impact fracture and all eight-by-eight contact iterations, but omit empty bond dispatches and structural-load shader branches for worlds without explicit members/bonds. Conservative host-owned graph-presence tracking must activate on authoring and snapshot restoration; command-cache keys must track transitions. Compare exact rigid states and live/restored structural reactions, run graphical full-frame measurement, preserve dynamic body capacity.

