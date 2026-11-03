---
id: ctlo-iit1
status: closed
deps: [ctlo-ldb7, ctlo-18l4]
links: [ctlo-8b67]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Expose energy-based shock ignition and conductivity interfaces

Define material/water conductivity and deduplicated electrical-energy exposure events so planned lightning can interact with reactive state without being required for ordinary fires.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Synthetic conductor/water contact events apply their declared finite heat/shock energy once, affect ignition according to material and wetness, and survive owner changes/reload without replay. Insulating and wet controls use explicit profiles, not a universal shocked-is-flammable flag. Full bolt generation and electrical-network propagation remain separately deferred.


## Notes

**2026-11-03T04:52:02Z**

Implemented af7c5deb on RED 2bd033c5. New interface fr_reactive_expose_shock (include/ferrum/fire/reactive_state.h): declared strike energy is admitted into the existing pending_shock_energy ledger through profile->electrical_conductivity, mixed with water conductivity by explicit saturation (mixture rule (1-sat)*material+sat*water). Insulating stone receives its 1e-4 profile share and stays cold; saturated wood conducts near water; combustion consumes deposited shock exactly once with wetness-aware ignition rates unchanged. Strikes survive fr_reactive_state_split proportionally and persist across fr_reactive_snapshot_write/read without replay; duplicate/replayed application is the bolt stage's event bookkeeping (ctlo-ddvz owns generation and network propagation as deferred). shock_exposure_tests 6/0; reactive_state 13/0, combustion 18/0, reactive_material 11/0 all pass. No universal shocked-is-flammable flag: strikes only add finite heat.
