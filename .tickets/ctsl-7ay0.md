---
id: ctsl-7ay0
status: in_progress
deps: [ctpc-wezp]
links: []
created: 2026-11-26T13:27:27Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, water, tests]
---
# Verify the siege burn-rate comparison with matched submerged loads

The siege douse test reports zero bound water on ordinary oil after soaking and during the sampled window. It compares absolute late-window burn on different post-ignition inventories and reports a twofold depletion-time guarantee. On the finite-ledger build it measured 116859 Greek and 66595 oil Q16 late-window fuel (1.75), versus 115944 Greek and 56765 oil (2.04) on the pinned predecessor 109361df. Both controls completed the full window.

## Design

Use the real authored pool and simulation to establish equivalent exposure and matched usable fuel after soak. Record submersion and bound water over the comparison window, and measure relative depletion from actual inventory. Preserve the finite gas, heat and water ledgers.

## Acceptance Criteria

Show the ordinary-oil load is actually doused and the Greek load keeps burning under equivalent submerged exposure. Prove the half-time requirement with matched usable fuel across repeated locked runs and verify complete simulation intervals. Keep the current guard active until the revised fixture proves the same requirement.

## Notes

**2026-11-26T13:44:43Z**

RED reproduced lost pre-ignition film, ignition erasing measured oil moisture, and a pool load moving with the launcher pouch. The authored stone basin now retains a stationary load, with water supplied by simulation and equal surface fuel instead of buried, gravity-shedding stock. The one-second Vulkan preflights passed with 327680 Q16 cold Greek film, 36/36 immersed oil faces, and exact preservation of bound water at ignition. In the longer wood-core control, oil held all 327680 Q16 fuel through 30 s, but its soak lowered adjacent liquid fill below the 75% per-cell coverage threshold. The longer comparison now reads both controls after soaking, samples the water ring and bound-water ledger each second, and retains the twofold late-window guard. Repeated time-to-depletion acceptance remains open.
