---
id: ctowpe-1y1n
status: closed
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

## Notes

**2026-11-27T18:49:23Z**

The mixed water/grain room regression failed only its per-owner stage expectation before the change and passed on the RTX A2000 after it. Serial and batched replay agree on all mapped cell, face velocity, live marker, particle, bulk and color state and confirmed timestamps. Grain admission on an additional page activates the previously empty owner. Production timings and the full suite at 8abb9206 are scheduled on hidden :93.

**2026-11-27T18:58:17Z**

Post-fix half-native camera run on hidden :93, 16 owners and 60 measured frames per band: near 24.263/38.107, middle 24.554/44.956, far 24.440/44.747 ms mean/p95. Every band advanced 2.000000104 seconds. Means pass and p95 remains over 33.333 ms. Full native run is in progress.

**2026-11-27T19:01:41Z**

Full native on the RTX A2000, 1920x1080 FIFO under hidden Xephyr :93: near 27.627/41.350, middle 27.503/47.327 and far 27.594/46.321 ms mean/p95. Each band ran 60 frames with all 16 owners active and advanced 2.000000104 seconds. Both camera modes are measured. Skipping idle grains improved the previous p95 in every band; ctowpe-vtlh still tracks the 33.333 ms limit.
