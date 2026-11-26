---
id: ctsl-7ay0
status: in_progress
deps: [ctpc-wezp, ctpc-xa7p, ctpc-9yer]
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

**2026-11-26T13:52:45Z**

Vulkan standing-pool comparison at 68b693be passed 1/1. Both porous-stone loads completed a 3 s soak with 327680 Q16 usable surface film, 56416 Q16 naturally bound water, and 36/36 wet faces. Over 30 complete post-strike seconds every face stayed wet in both cases. Oil retained all film and 63734 Q16 bound water. Greek retained 63732 Q16 bound water and consumed 99040 Q16 fuel, including 45080 in the 15–30 s window. Greek passed 15% depletion by 14 s while oil retained 100% through 30 s. This establishes the late-window guard and a measured partial-depletion milestone. Repeated locked runs to the depletion target remain required. Dry staging, three pool preflight regressions, five CPU coating tests, and three GPU conservation tests passed. The pinned full suite is queued behind its predecessor.

**2026-11-26T14:01:59Z**

The second matched 30-second replay passed with byte-for-byte identical telemetry. The stationary test setup stays confined to the douse case. Holding the load against the corrosion samples caused an invalid oxidizer diagnostic and missing gold record, reproduced on 69ba82e4 and tracked by ctss-cf7j. Restoring the moving load for the existing corrosion case passed with exactly the parent revision output, including the gold fraction 0.0269 after reload. All six CPU corrosion checks passed.

**2026-11-26T15:35:14Z**

The finite mixture debit now rounds proportionally at the real simulation cadence. A distinct terminal precision bug still leaves 2 oil, 2 jelly and 7 bound oxidizer Q16 after 900 simulated seconds at 60 Hz. ctpc-9yer tracks terminal fuel settlement before the complete time comparison. The existing 30-second matched submersion guard remains in use.

**2026-11-26T16:08:56Z**

The terminal kinetics regression is closed. The updated 30-second pool replay completed both controls with matched starting film of 327680 Q16, matched initial bound water of 56416 Q16 and 36 of 36 wet faces. Greek retained 228620 Q16 fuel, while oil retained all 327680. The existing late-window guard passed at 45080 versus zero. The repeated comparison through actual depletion remains open.
