---
id: ctlo-ibli
status: in_progress
deps: []
links: []
created: 2026-10-24T08:39:59Z
type: task
priority: 1
---
# Scalarize four-vertex contact geometry without changing ordered SAT

## Acceptance Criteria

GPU frozen oracle compares overlap, exact normals/depth, witness coordinates/features across rotated quads and general polygons. Quad fast path uses no dynamic polygon loops, preserves all eight SAT axes and their order. Verify actual displayed frame time and full contacts/concave dynamics.


## Notes

**2026-10-24T08:45:33Z**

RED32768 ordered quad cases preserve exact results but execute up to54 dynamic projection iterations. GREEN actual core/synchronization oracle32768/32768 exact overlap, normal/depth, witness points/features with zero dynamic projection iterations. Ordinary displayed500-frame brush run30.150ms mean41.552ms worst, improved average but frame target remains unmet; early repeated large cuts cost5-9ms.
