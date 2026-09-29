---
id: dun-7l4n
status: closed
deps: []
links: [dun-9det]
created: 2026-09-29T04:11:37Z
type: feature
priority: 1
assignee: kmd
tags: [mpm, materials, fluid]
---
# Make sufficiently wet dirt flow as cohesive mud instead of powder

## Design

The existing GPU MPM path marks large wet dirt components with DC_MPM_MUD_FLAG and lowers their shear limit, but their motion still resembles independent grains. Give mud a viscoplastic continuum response with sustained lateral flow and cohesive deformation while retaining particle-based coupling to Eulerian water. Keep constitutive forces, chunk crossings, and moisture transfer on the GPU.

## Acceptance Criteria

A sufficiently wet connected dirt patch becomes mud and spreads downhill and sideways on a supported slope or under its own head; the same dry dirt forms a granular pile. The test measures the wet patch extending farther and retaining a connected body over several simulated seconds. Dirt mass and bound-plus-Eulerian water are conserved across material transitions and chunk seams; drying restores granular behavior with hysteresis. Add an interactive mud-and-water test scene plus automated before/after image or state checks, and benchmark the GPU tick cost at half-native resolution without per-frame CPU physics readback.


## Notes

**2026-09-29T05:09:58Z**

GPU MPM mud uses signed cohesive pressure, moisture-dependent shear yield, plastic relaxation, and no mud-to-mud granular impulse. Tests: dry span 7 cells, wet span 51, largest connected wet body 45/64 particles after 120 ticks; exact dirt and bound/free water mass; drying hysteresis. make test, make test_ui, make test_half_native pass. Half-native mud benchmark: 61.32 presented ticks/s; 2.998 ms GPU MPM stage.
