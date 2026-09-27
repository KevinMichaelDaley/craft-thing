---
id: dun-v75v
status: closed
deps: []
links: []
created: 2026-09-27T01:59:30Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, fluid, components]
---
# GPU connected dirt components and wet granular response

Sand should settle and retain pore water; small connected dirt patches should separate in water while larger connected patches form mud.

## Design

Label 4-connected dirt cells across resident chunk seams on GPU, classify component size, and use it to select loose sediment versus mud MPM behavior. Preserve exact Eulerian plus bound water accounting and leave labels reusable by future rigid-body extraction.

## Acceptance Criteria

Wet sand retains mass and settles, small wet dirt patches break apart, large wet dirt components form mud, seam-spanning components have one label, and the combined interactive demo shows the behavior with no normal-frame readback.


## Notes

**2026-09-27T02:19:53Z**

Implemented GPU root-hooked 4-connected dirt labels with GPU indirect early termination; wet sand conserves pore water and sinks; small wet dirt fragments separate; components >=8 wet grains become mud. All headless, UI smoke, and Vulkan validation tests pass. Future rigid extraction tracked by dun-spbq.
