---
id: ctlo-r2ap
status: open
deps: [ctlo-18l4]
links: [dun-u68k, ctlo-h9of, ctfs-dmxv]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Implement sparse variable-density smoke oxygen and gas transport

Add separate gas species mass, energy, pigment and velocity in available world-cell volume, with buoyancy, sideways mixing and real solid/liquid apertures. Gate production on a reference-validated weakly compressible model or documented expansion-aware alternative.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Prove sealed heating/compression/expansion and CFL stability before selecting GPU pressure/substep policy. Conservative species/energy/color flux, oxygen consumption versus inert-smoke dilution, ventilation and extinction controls pass. Differently colored plumes mix by admitted carrier/soot amount. Camera edges/cold pages cannot replenish air or discard smoke. Include finite compressed-pocket retention and pressure-driven liquid displacement for submerged emissions; Greek fire must not stop immediately solely because a cell initially has zero free gas volume. Sparse work grows without arbitrary global caps; report actual frame cost.

