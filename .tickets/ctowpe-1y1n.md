---
id: ctowpe-1y1n
status: in_progress
deps: []
links: []
created: 2026-11-27T18:39:30Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Skip idle grain work in confirmed particle-free sealed room batches

At d50c4f60, MPM consumed 16–22% of sampled owner stage time in the saturated water-only room benchmark. Avoid this work only where the actual state establishes that no grains can be advanced, without changing resident or shared-seam physics.

## Acceptance Criteria

Prove the grain stage is omitted for empty independent rooms, retained for grain-bearing rooms even when another owner is empty, and activated after re-admission of grain stock. Compare full physical state and confirmed time with scalar execution. Keep populated and shared-seam behavior, and measure both camera modes under hidden Xephyr.
