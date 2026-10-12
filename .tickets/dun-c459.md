---
id: dun-c459
status: in_progress
deps: [dun-jo5i, dun-x9ei]
links: [dun-spbq, dun-sbk9, ct-ha8q]
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


## Notes

**2026-10-12T19:50:44Z**

Starting RED coverage for final-occupancy displacement: raise water into resident space conservatively; retain and explicitly report unresolved volume behind solid/cold boundaries. Runtime transfers remain GPU-only and diagnostics are explicit readbacks.

**2026-10-12T19:59:15Z**

GPU final-occupancy displacement now conservatively raises water into resident column capacity using deterministic integer transfers. Solid/cold barriers retain unresolved volume at its world cell, explicitly reported by diagnostic API and preserved in chunk snapshots. No ordinary-frame readback. Full headless232/232 and quarter-capacity fluid31/31 pass; SPIR-V validated, zero Vulkan validation errors. Lateral routing is deliberately deferred; blocked-column volume is explicit overflow.

**2026-10-12T20:33:59Z**

Reopened while buoyancy integration extends the boundary from whole-cell masks to exact solid cell volumes and face apertures. Added RED for partial-cell flow through a fully covered face; GREEN adds weighted projection/transport and row-first rezoning into vacated capacity. Full regressions pending before reclosure.
