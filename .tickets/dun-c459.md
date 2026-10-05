---
id: dun-c459
status: open
deps: [dun-jo5i, dun-x9ei]
links: [dun-spbq, dun-sbk9]
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid, rigid]
---
# Couple Eulerian fluid to moving rigid occupancy

Apply final rigid occupancy as a fluid boundary and define displacement handling when a body enters a wet cell.

## Acceptance Criteria

No fluid silently disappears as a body moves; displaced volume is redistributed or explicitly reported as overflow; behavior is reproducible.

