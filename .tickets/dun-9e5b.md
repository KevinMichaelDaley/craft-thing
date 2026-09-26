---
id: dun-9e5b
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, fluid]
---
# GPU Eulerian fluid simulation stage

Simulate fluid volume and flux at fixed world cells on GPU after rigid bodies and before falling-sand rules. Respect terrain/body boundaries and chunk edges.

## Acceptance Criteria

Closed-basin mass is conserved within fixed-point tolerance; impermeable cells block flow; cross-chunk transfers conserve mass; moving-body displacement has an explicit tested policy.

