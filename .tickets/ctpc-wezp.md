---
id: ctpc-wezp
status: closed
deps: []
links: []
created: 2026-11-26T12:42:35Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, gas, conservation]
---
# Preserve submerged coating products in the finite gas ledger

The Greek-fire demo change 09f5546c added a shortcut in shaders/reactive/coating_burn.glsl that drops exhaust, soot, and the gas share of reaction energy when the chosen receiver has zero free volume. The rigid-body submerged coating fixture spends fuel and bound oxidizer but observes zero products in the four surrounding water cells.

## Design

Use the existing finite zero-void gas ownership and displacement machinery from ctlo-r2ap and the reference reaction ledger. Trace packet publication, pressure work, and save/load before replacing the discard branch.

## Acceptance Criteria

The submerged terrain and rigid coating cases conserve consumed fuel, bound oxidizer, products, and energy through reaction, transport, and checkpoint reload. The zero-void packet stays valid and bounded, and ordinary oil, dry combustion, and water stock remain correct. Reuse the existing submerged test and add a regression for pending inventory.


## Notes

**2026-11-26T12:43:23Z**

The exact pinned predecessor 1d0fc902 reproduced the isolated test with DC_GREEK_BODY_ONLY=1 at tests/vulkan/combustion_body_feedback_tests.c:305. The pose change initially produced the same failure, with the other 10 rigid thermal/combustion feedback cases passing.

**2026-11-26T12:45:09Z**

The earlier conservative implementation c16a1881 published the gas share even at zero geometry volume. Existing gas_geometry_cases.h tests cover zero-void nucleation and sealed pending stock. Reuse these invariants to verify restored publication from the reaction passes, since those tests currently inject a source packet directly.

**2026-11-26T13:28:02Z**

Completed the finite submerged ledger. RED 66ee48aa measured 78 fuel and 32 oxidizer consumed with zero retained product gas. GREEN 5a877064 publishes the gas share and removes the wet-film water deletion and unbudgeted heat. RED 63907492 reproduced the disk validator rejecting pending stock. GREEN bdd327d8 preserves it in the existing checkpoint format. RED 947df40b reproduced nonfinite gas under gravity and wind. GREEN d3f96759 keeps pending gas out of buoyancy and pressure until a void opens. Final terrain and rigid checks pass 3/3, gas checks 3/3, and world/thermal/reactive/gas streamer checks 16/16. The sealed reaction retained 104 Q16 gas units at zero volume, matched its 14.923096 kJ heat ledger, reloaded, and displaced 30446 Q16 water with 5.81e-7 kJ drift. Eleven rigid feedback cases, four player GPU cases, ordinary oil/oxygen controls, sparse-owner verification, CPU references, and SPIR-V validation passed during development. The siege run completes both controls after the gravity fix. Its separate late-rate comparison remains open under ctsl-7ay0 with the pinned predecessor comparison and oil wetness evidence recorded there. The final full suite is queued behind the earlier pinned revisions.
