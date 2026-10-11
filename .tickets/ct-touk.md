---
id: ct-touk
status: open
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
