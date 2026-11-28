---
id: ctowpe-attc
status: closed
deps: []
links: []
created: 2026-11-28T05:21:57Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, replay, fluid]
---
# Eliminate cross-workgroup bulk-flow velocity races in restored mixed-liquid rooms

The parked-room reopen oracle intermittently publishes a different face velocity on cell 2208 for the second room. Fresh traces show identical timestep endpoints, independent serial submissions, and the mismatch already present immediately after the second GPU tick. Bulk-phase transverse shear reads velocities that adjacent workgroups can overwrite within the same parity.

## Acceptance Criteria

Exact repeated GPU mixed-liquid cells, face velocities and phase state; unchanged constitutive shear, momentum, volume, heat and retained-volume checks; repeat actual parked-room reopen comparison.


## Notes

**2026-11-28T05:38:33Z**

All 22 bulk material tests passed after separating face writes from transverse shear reads, but a parked-room replay still failed. Five stage snapshots from a failing physical tick showed identical cells, bulk and faces through rigid processing. A single pressure and two velocities first differed inside the first scheduled water run, before any bulk exchange. Generic pressure also reproduced the mismatch. A new command-recording regression exposed the damped velocity write at the end of a water cycle followed by the next projection snapshot without a compute write-to-read dependency. The added barrier passes the ordering regression; repeated exact replay remains under test.

**2026-11-28T06:46:57Z**

The integrated native snapshot suite now passes the parked-room reopen comparison for mixed water, oil and slime. Sixteen additional exact replays passed across specialized and generic pipelines. Pressure and bulk suites passed 10/10 and 22/22. The integrated total was 44 passed and one separate saturated camera admission timeout, tracked as ctowpe-qtwp; that case also passed when run alone from the identical binary.
