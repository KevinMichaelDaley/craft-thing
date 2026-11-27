---
id: ctowpe-bsh0
status: open
deps: []
links: []
created: 2026-11-27T18:58:05Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-vtlh
---
# Reuse pressure tile and stencil preparation across consecutive solver phases

When several water phases are recorded consecutively in one tick, the pressure passes rescan tiles and regenerate the aperture stencil at each phase boundary even though there is no intervening edit, body motion or water transport. Check the real replay command stream and reuse eligible preparation while retaining the complete pressure solve.

## Acceptance Criteria

Count native GPU work-list and stencil dispatches for adjacent pressure ranges and for phases separated by a paint, page change or rigid tick. Require byte-exact pressure, velocity, cell stock and marker equivalence against independently prepared phases, including residual pressure on dry tiles. Preserve visible and coupled physics and compare production camera frame times on hidden Xephyr.

