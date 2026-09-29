---
id: dun-7l4n
status: open
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

