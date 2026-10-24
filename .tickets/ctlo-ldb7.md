---
id: ctlo-ldb7
status: closed
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


## Notes

**2026-10-24T22:44:58Z**

RED87bd51e compiled8 failing profile cases; GREEN15470c5 passes all8 with a generated128-byte physical-only table, explicit old body/terrain mapping, wetness/oil/oxidation/shock/oxygen modifiers and independent substrate/coating hazards. Additional RED967c7d9 exposed a draft recipe energy bug:1 mass unit of inputs held8200 energy but compound claimed11200. Explicit balance revision lowers Greek fuel energy to11714.285714 per fuel mass, preserving every gameplay/performance threshold and all ingredient quantities; resulting mixture energy8200=8200. No new reaction has run yet. Hardware profile-transfer/visual-independence probe and full regressions next.

**2026-10-24T22:53:57Z**

Final profile milestone:9 C reference cases,6 Python fixture/codegen checks, and256 real Intel Vulkan cases pass. Core+synchronization validation reports no layout/rate errors and pigment changes leave physical outputs bit-identical. Full make test reports670 C passes and only2 pre-existing ctr-ojvy failures, zero compiler warnings. New 128-byte physical table has no color fields; runtime burning/gas/phase movement remain downstream. Explicit recipe energy correction is documented in design/reactive_material_profiles.md. All coefficients generated from the versioned manifest; legacy IDs/densities preserved.
