---
id: ctlo-ibli
status: closed
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

**2026-10-24T09:01:49Z**

Extended actual core/synchronization frozen oracle passes32768 rotated quad cases plus32768 mixed pentagon/quad cases, exact hit/miss, normal/depth, witness coordinates and feature IDs. Quad dynamic projection iterations remain zero.

**2026-11-28T04:39:43Z**

Repeated the frozen comparison under core and synchronization validation on the RTX A2000. Before correction, 1,943 of 32,768 rotated-quad cases disagreed; one large-coordinate case selected the wrong contact face. After correction all six 32,768-case families agree exactly on hit, normal, depth, witnesses, and feature IDs. The quad projection retains zero dynamic polygon iterations. Production contacts and displayed concave dynamics are being exercised separately.

**2026-11-28T04:53:29Z**

All ten production contact tests passed with core/synchronization validation, covering rotated, mixed, partial, symmetric, overflow and missing-page cases. The displayed 480x270 two-body concave test passed 180 ticks in 3.233s including pacing; both bodies rotated, made body and terrain contacts and retained exact mass, with four byte-identical paused frames. Separate four-light/sun renderer scene ran 90 displayed frames in 0.907s. This complements the 196,608 exact GPU oracle comparisons.
