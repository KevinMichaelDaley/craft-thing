---
id: dun-x9ei
status: open
deps: [dun-0p7p, dun-cwhn, dun-9qub]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid]
---
# Solve rigid contacts against terrain and bodies

Generate contacts from nearby material/occupancy cells, resolve impulses/penetration in bounded iterations, and document speed/substep limits.

## Acceptance Criteria

Body rests on terrain, reacts to removed support next tick, and does not penetrate under supported bounds; body-body overlap is resolved.

