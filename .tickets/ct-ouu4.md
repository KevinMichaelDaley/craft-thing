---
id: ct-ouu4
status: open
deps: [ctlo-h9of]
links: [ctr-ebl5]
created: 2026-11-01T18:11:14Z
type: feature
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [renderer, vulkan, lighting, performance, fire]
---
# Stage radial lighting propagation across frames within the displayed frame budget

Schedule finite-range radial point and fire transport over multiple display frames so optional propagated lighting remains responsive in the bonfire and catapult demos without reducing physics fidelity or resolution.

## Design

Use bounded active-source/tile work and persistent authoritative light fields. Partition work by dirty source, optical, and visible-region revisions; preserve a coherent published field while a new revision is built. Permit bounded temporal convergence but no stationary artifacts, long straight rays, stale light after fire extinction, or uncontrolled recomputation. Reuse existing sky, point, and aggregate fire semantics.

## Acceptance Criteria

RED-GREEN-REFACTOR tests prove finite radial falloff and exact maximum range, opaque and unknown blocking, local soft shadows, colored moments, correct dirty invalidation for ignition/extinction/moved sources/optical edits/camera movement, and eventual convergence of staged output to the single-revision reference. No flicker or dark flash during revision rollover; paused frames are byte-stable. Run rendered ordinary and Greek fire plus catapult captures on this machine; check temporal lighting as flames change. Benchmark complete native-display quarter-native bonfire and catapult 1000-frame runs, including simulation and render, against the current 62.229ms lit bonfire mean / 68.650ms worst and require <30ms total displayed frame time without reducing physics fidelity or visual resolution. Run full make test and push.

