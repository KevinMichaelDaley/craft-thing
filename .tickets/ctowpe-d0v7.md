---
id: ctowpe-d0v7
status: in_progress
deps: []
links: []
created: 2026-11-27T18:26:39Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
---
# Advance granular particles by their actual elapsed world time

The archived 73475311 grain solver scaled motion and moisture with elapsed time. The current MPM command sequence always records two fixed half-tick substeps, including when offscreen replay confirms a longer or shorter interval.

## Acceptance Criteria

Compare equal-time granular trajectories and stable particle stock with the Vulkan solver. Preserve fixed 60 Hz behavior, shorter intervals, bounded longer substeps, moisture conservation, existing shared-grain seam behavior and correct cached commands when duration changes.


## Notes

**2026-11-27T18:46:07Z**

At 9daa0546, synchronization-validated RTX A2000 runs on hidden Xephyr :93 passed all three equal-time, half-dose, and half-travel cases with the MPM cache enabled and disabled. The shared cohort passed all three grain-crossing cases including multi-interval parcel identity, and all four grid-fusion variants matched particle and grid state. The complete particle executable and aggregate suite are still pending.
