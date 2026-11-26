---
id: ctpc-xa7p
status: closed
deps: []
links: []
created: 2026-11-26T15:28:45Z
type: bug
priority: 1
assignee: kmd
parent: ctsl-7ay0
tags: [fire, coating, vulkan]
---
# Keep mixed coating debits proportional at simulation cadence

Each one-unit reaction rounds the oil share down, charging the entire debit to the jelly. Repeated fine steps deplete the jelly early and abandon oil while the host is submerged and bound oxidizer remains.

## Design

Round the proportional fuel split to the nearest representable whole unit, clamped to available species. Preserve the existing reaction rate and complete fuel, oxidizer, product and enthalpy accounting.

## Acceptance Criteria

At the 60 Hz simulation cadence, a submerged balanced film continues consuming both species when reaction steps admit only one whole unit. CPU reference and Vulkan record the same species balances and close their finite product ledger.


## Notes

**2026-11-26T15:35:14Z**

RED at 60 Hz spent all 32 jelly units and zero oil in 120 simulated seconds, stranding 19 oxidizer units. The corresponding Vulkan single-unit dispatch left oil 4, jelly 3. After rounding the proportional share to the nearest unit, the CPU retained 8 oil and 9 jelly and closed the finite mass and enthalpy ledgers. Vulkan retained oil 3, jelly 4 and closed the submerged product ledger across checkpoint and gas transport. Six CPU Greek-fire and six focused Vulkan rigid-coating checks passed. Exact 32-bit shader division of the extended product matched 40020 CPU integer reference cases. Terrain and body shaders produced identical stripped SPIR-V before and after the refactor.
