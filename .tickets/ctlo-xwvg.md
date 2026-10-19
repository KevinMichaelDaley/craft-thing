---
id: ctlo-xwvg
status: in_progress
deps: []
links: []
created: 2026-10-19T21:46:40Z
type: task
priority: 1
parent: ctr-ebl5
tags: [vulkan, performance, rigid]
---
# Gate rigid grain-bed work from GPU body and particle activity

Strict full-frame tail remainsRED: first640frame43-51ms, with rigid solver12ms versus2.5ms warm; grain-bed init/pointer doubling runs over the full grid even with no particles or active bodies. Add a one-group GPU planner and owner-specific indirect work commands; no CPU pixel walk/readback or fidelity reduction.

## Acceptance Criteria

GPU counters gate grain-bed init/jumps to zero for an empty particle pool or no active bodies, and immediately restore full work when bodies and grains are present. Empty rigid per-body phases may use GPU body-work triplet. Preserve pressure-bearing powder interactions, exact physics tests, dynamic bound inputs, core/sync validation, graphical capture and strict<30ms full-frame gate.

