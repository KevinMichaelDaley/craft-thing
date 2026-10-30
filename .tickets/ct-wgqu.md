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

**2026-10-30T17:59:44Z**

Additional NVIDIA 60s fixture trials, all reverted: 32-cell coal bed + middle-right x164->160 passed 8/8 at21s, wet0/8, but max gas reached 9122K (plasma-like, unacceptable). 16-cell right bed + shift reached7/8 with max gas~1691K; 24-cell bed + shift reached7/8 with max gas~2948K. Lowering middle-right y78->80 gave2/8; raising y78->76 gave5/8. Core heat/oxygen transport needs correction; do not ship brute-force fuel.

**2026-10-30T18:15:02Z**

Corner-path TDD: focused rigid-body feedback RED for diagonal hot gas and diagonal-only oxygen; GREEN8/8 tests with finite oxygen/energy and wet controls. Original 8-cell starter fixture passed 8/8 dry0 wet twice at21s, but third run stopped7/8 by60s with one log zero fuel, so overall reliability remains RED. Wider starter with same total fuel passed once but failed repeat7/8 and max gas~5021K; reverted. Shader corner path retained, scene fixture unchanged. Half-native performance after corner heat before oxygen change was32.740ms mean42.003ms worst; full regression underway.

**2026-10-30T18:31:57Z**

Final corner-path checkpoint: full make test PASS and focused combustion_body_feedback_display_tests8/8 with diagonal-only oxygen. Original 8-cell fixture repeated displayed runs PASS8/8 at21s twice, FAIL7/8 at60s once; wet0/8 all. The body-to-gas corner path is physically bounded but not sufficient for deterministic all-log ignition. Full-width coals at unchanged total fuel also passed once and failed repeat7/8 with max gas~5021K; reverted. Keep ticket in_progress and original fixture unchanged.

**2026-10-30T21:02:07Z**

2026-10-30 RTX A2000 X11 baseline 64-lane build, tools/smoke_profile, three consecutive full displayed bonfire_display_tests runs PASS8/8 dry0/8 wet at58.5,55.5,55.5 simulated seconds. Max gas about1560K,1372K,1165K, no plasma. Earlier identical-layout runs intermittently failed7/8, so this is reassuring but no root-cause change; keep ticket open pending deterministic coverage or stronger repeated evidence.
