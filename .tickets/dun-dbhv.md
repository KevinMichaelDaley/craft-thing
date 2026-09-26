---
id: dun-dbhv
status: open
deps: [dun-jo5i]
links: []
created: 2026-09-26T06:42:29Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid, markers]
---
# Add sparse virtual markers for Eulerian water interfaces

Keep Q16.16 water volume on fixed GPU cells. Seed a bounded 2-4 massless markers per interface cell, advect with Eulerian face velocity, reconstruct thin free-surface features, and accept only equal-and-opposite conservative correction transfers. Stream marker ownership with world chunks. Follow design/fluid_solver.md.

## Acceptance Criteria

Markers remain sparse and deterministic; on/off thin-sheet and splash captures demonstrate reduced interface smearing; total grid water volume remains exactly conserved in closed scenes; marker state survives chunk eviction and reload; the window can display a marker debug overlay.

