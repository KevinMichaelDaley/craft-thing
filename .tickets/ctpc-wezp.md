---
id: ctpc-wezp
status: open
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
