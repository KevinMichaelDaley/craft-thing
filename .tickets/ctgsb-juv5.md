---
id: ctgsb-juv5
status: in_progress
deps: []
links: [ctgc-tteu]
created: 2026-11-26T20:40:07Z
type: bug
priority: 1
assignee: kmd
tags: [gas, physics, vulkan]
---
# Preserve saturated pigment when reading compensated gas stock

The 8e45829e compensated carrier representation retains the exact authored carrier while pigment occupies one float. A saturated channel such as 0.1 rounds to a float above the exact carrier and makes the cold seed and reference transport reject a valid packet.

## Acceptance Criteria

Seed fully saturated exhaust and mixed-carrier channels, return physically valid saturated color, round-trip their unchanged packet bytes through checkpoint import, and continue rejecting genuine over-carrier input.


## Notes

**2026-11-26T20:42:46Z**

A native Vulkan regression compiled and failed at fr_gas_gpu_seed_chunk on physically valid fully saturated exhaust and mixed exhaust/soot channels. The unaltered full-suite prefix also failed five existing colored transport, seam, and valve cases at seed admission.
