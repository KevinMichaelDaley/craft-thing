---
id: ct-wgqu
status: open
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

