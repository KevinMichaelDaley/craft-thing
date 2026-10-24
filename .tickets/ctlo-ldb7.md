---
id: ctlo-ldb7
status: open
deps: [ctlo-wmy2, ctlo-ck1i]
links: []
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Define extensible reactive material and phase profiles

Add explicit material reaction/thermal/electrical/rheology profiles and namespace mappings. Append new material IDs without changing existing ones. Sticks may initially use thin wood geometry.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

RED-GREEN-REFACTOR tests cover wood/dense wood/leaves/leafbits, coal/char, nonfuel stone, ash, oil, red-slime jelly, Greek compound and corrosion residues. Wetness suppresses ordinary ignition, combustible coatings burn on inert hosts, oxidation response is material/reagent-specific, conductivity is independent of fuel, and magic color is independent of heat. Reject invalid profiles and preserve prior defaults.

