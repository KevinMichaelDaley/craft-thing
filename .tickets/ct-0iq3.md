---
id: ct-0iq3
status: closed
deps: [ct-il9b, ct-i5a4]
links: [dun-spbq]
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Timed support failure and atomic structural-to-rigid conversion

Accumulate material-dependent overload damage and confirmed support-loss time in simulation seconds. On failure, cut failed joints, relabel islands and atomically reserve/commit rigid outputs while keeping rooted islands structural. Reuse common geometry/material-volume conversion contracts with dun-spbq without making dry structural support depend on unfinished water behavior.

## Acceptance Criteria

Brief impact or temporary support loss below the configured grace does not trigger topology failure; sustained overload, anchor removal or an actual mechanism does. Different frame cadences produce equal failure time. Missing chunks and numerical UNKNOWN do not age failure. A sound unsupported island becomes a freely falling rigid assembly; sound connections remain bonded. Geometry, material volume, mass and linear/angular momentum are preserved within documented tolerances; concavities remain empty. Full output pools roll back conversion without ghost copies, deleted supports or lost mass.


## Notes

**2026-10-11T23:08:07Z**

Implemented fixed-tick material grace, permanent p=2 damage, finite anchor/joint cuts, rooted survivor retirement, atomic sound-island conversion with reserved bonds and reused body slots. Vulkan validated 13 failure tests in headless and quarter-native; full headless 194/194 and full quarter-native passed. Real Wayland one-tick+idle vs five-tick presentation batches both convert at 90 ticks/1.500 seconds. Objective bearing Jacobian and isolated GPU momentum ledger covered by angular/energy regressions; pool saturation remains pending with ownership/mass intact. GPU indirect bond dispatch restores static 16-member quarter timing to 8.337 ms rigid/structure, 17.643 ms whole. Localized material fracture remains ct-gf6q, persistence ct-1n9n; ct-fan4 tracks an earlier unreproduced visual SIGSEGV. UI validation and final push pending.

**2026-10-11T23:09:15Z**

Final UI battery also passed with real Wayland presentation and concave-body visuals. SPIR-V validates for Vulkan 1.3. Localized fracture/crumble and graph snapshots deliberately remain open in their dependent tickets.
