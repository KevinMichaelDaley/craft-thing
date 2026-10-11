---
id: ct-touk
status: closed
deps: []
links: []
created: 2026-10-11T05:44:08Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Mechanical structural model, material profiles and analytic reference tests

Define 2D frame/continuum elements and bearing, pinned, bonded and moment-resisting joints with explicit units, geometry-derived sections, force/moment balance and material-derived mass. Create a CPU test oracle only, never a runtime fallback. See design/structural_bodies.md.

## Acceptance Criteria

Cantilever displacement FL^3/(3EI), root shear F and moment FL; rotated frame equivalence; pinned mechanism vs stable triangulated brace; finite bearing/pull-out capacity; compression/tension asymmetry, buckling and mesh refinement without hop-count strength loss. TDD tests compile and fail before implementation; calibrated synthetic coefficients are labeled as game parameters.


## Notes

**2026-10-11T05:47:24Z**

Define explicit terrain/rigid/structural material ID mappings. Current rigid-body WOOD and terrain SAND share a numeric value in distinct namespaces; marking or conversion must not reinterpret IDs. Placed wood may use its body profile before wood terrain cells exist. Mechanical refinement must preserve volume, mass, buckling length and fracture energy.

**2026-10-11T05:58:17Z**

Implemented shared structural profiles/units, explicit terrain/body ID mapping, rectangular geometry sections/mass, finite bearing/pinned/bonded/moment tangent modes and capacity checks. Test-only dense rotated Timoshenko/truss oracle rejects mechanisms; 10 analytic tests cover cantilever force/moment/displacement, rotation/refinement, brace stability, shear deformation, compression/tension asymmetry, physical-span buckling and atomic invalid-input rejection. All 143 pre-existing headless cases and 10 new cases pass under Intel Vulkan validation. Reference solve is never linked into runtime; GPU active sets and coupling remain ct-il9b.
