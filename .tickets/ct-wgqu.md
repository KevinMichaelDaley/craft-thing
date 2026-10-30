---
id: ct-wgqu
status: in_progress
deps: []
links: []
created: 2026-10-30T16:27:49Z
type: bug
priority: 0
assignee: kmd
parent: ctfs-5gil
tags: [bonfire, combustion, vulkan]
---
# Restore autonomous ignition of all eight dry bonfire logs on NVIDIA

With current RTX A2000 X11 and tools/smoke_profile, bonfire_display_tests reaches 60 simulated seconds with only 5/8 dry logs caught (device gas state) and 4/8 (mapped state), while wet stays 0/8. Faster memory placement changed displayed cadence but both state placements fail, so device-state option is not the sole cause. Some logs receive no fuel usage, indicating exposure/oxygen or rigid placement path needs investigation. Existing ctfs-6t7k was closed under previous cadence.

## Acceptance Criteria

RED test reproduces 60s all-log failure on NVIDIA normal displayed cadence; GREEN all eight dry logs autonomously catch and sustain moving flame, wet logs stay 0/8, finite fuel and oxygen accounting hold, and save succeeds. Do not weaken the all-log assertion.


## Notes

**2026-10-30T17:05:18Z**

2026-10-30 NVIDIA fixture trials: moving the eight-cell starter to a 2x4 open channel reached only 4/8 dry logs at 60s; distributing the same eight cells under three lower log groups reached 5/8. Both trials were reverted with no weaker acceptance gate. A 120s diagnostic is running to distinguish delayed spread from a permanent isolation path.

**2026-10-30T17:07:26Z**

120s diagnostic at original layout: dry ignition plateaued at 6/8 from ~34s through 120s; log indices 2 and 6 used zero fuel throughout. Wet remained 0/8. Gas near starter row26-29 showed very low or zero O2/mass in many cells. Therefore this is an isolation/transport issue, not merely a longer ignition delay. Diagnostic 120s test bound restored to 60s.

**2026-10-30T17:47:04Z**

Closer nonoverlapping logs experiment (lower-right x170->166, upper-right x166->162) worsened 60s result to 4/8 dry; wet0/8. Reverted. Gas near starter row26-29 had near-zero O2 by 60s, suggesting ventilation/exposure, not mere initial geometry distance. Per-log RED test now asserts saved checkpoint first, then each dry log caught.
