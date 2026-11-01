---
id: ct-bk2l
status: in_progress
deps: []
links: []
created: 2026-11-01T04:35:21Z
type: feature
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [smoke, wood, gas, visual]
---
# Allow a small smoke flux through burning wood obstacles

Burning wood currently deflects 2D smoke too strongly. Follow existing rigid-body smoke permeability treatment and allow a small conservative gas/soot flux through burning wood so the screen-space plume approximates flow around wood in the missing third dimension. Apply only to burning wood, preserve solid collision and heat/combustion behavior, and keep opacity density-dependent.

## Acceptance Criteria

RED GPU test shows blocked smoke at burning constructed and rigid wood. GREEN test verifies finite, small smoke transfer with species/pigment conservation and no leak through unburned wood or stone. Visual bonfire plume bends less around logs; dry/wet bonfire and full tests pass with frame budget.


## Notes

**2026-11-01T05:02:19Z**

Gas and body smoke bypass tests pass and 300-frame bonfire passes (16.96ms mean, 28.69ms worst). Leave open: 1000-frame bonfire exceeds strict 30ms worst gate by 0.565-0.905ms in repeated runs; frame API trace indicates intermittent display present waits. Physical bypass implementation is committed.
