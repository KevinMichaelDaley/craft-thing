---
id: ctlo-tk0j
status: in_progress
deps: [ctlo-r2ap]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Implement substrate-specific oxidation and magical corrosion residues

Track oxidation/corrosion exposure and conversion rules alongside thermal/reactive state. Ordinary material response and fictional Greek-fire destruction of valuables are explicit distinct profiles.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Oxygen/wetness/time controls and per-reagent rates conserve substrate-to-residue mass and weaken relevant sections progressively. Burned gold-like coins become specified rust-like magical residue; ingredients become spent ash with lost original potency/value. Provenance and partial surviving matter persist through pickup proxies, fracture and reload; residue cannot restore pristine items by recrafting or import.


## Notes

**2026-11-01T14:52:12Z**

CPU and terrain GPU corrosion implemented with finite oxidizer, spent residue, checkpoint save/reload, and partial split/snapshot. Full make test passes under persistent Xephyr :92; native quarter-native bonfire 1000-frame runs 18.456ms/25.358ms and 18.565ms/24.507ms mean/worst, both pass. Continuing body/proxy material support before closing.
