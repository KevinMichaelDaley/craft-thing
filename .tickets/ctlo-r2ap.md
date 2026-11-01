---
id: ctlo-r2ap
status: closed
deps: [ctlo-18l4, ct-pb6c]
links: [dun-u68k, ctlo-h9of, ctfs-dmxv, ct-ixii, ct-90z8]
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


## Notes

**2026-11-01T02:41:32Z**

CPU finite-pock​​et and GPU flooded-pocket displacement implemented and tested. One-quantum cut-cell stock is now routed conservatively. Displayed 1000-frame closed/open/carved runs meet frame budgets, but carved gas audit still fails at 16389 peak pressure in MPM-occupied 1/4096 cell (initial peak 133927 at 1/65536). ct-pb6c tracks physical cut-cell pressure relief/coupling; gas ticket remains in progress.

**2026-11-01T13:34:28Z**

Closed after finite zero-void submerged gas nucleation. RED/GREEN/REFACTOR commits 73cb11c/ceb4ac0/8740950 and 4fb8422/b2ee13c/23781ce add reference and Vulkan tests, pending zero-volume inventory validation, finite pressure work, water displacement against compressed receiving air, and sealed pocket retention across two GPU ticks plus checkpoint round trip. Focused GPU 2/2, reference 11/11, full make test pass. Isolated GPU open-pocket energy drift -2.08e-9 kJ; oxygen/water conserved. Native quarter-resolution smoke-box repeat passed all six 1000-frame runs: closed means 3.820/3.836ms, open 7.034/6.983ms, carved 9.026/8.924ms; carved worst 20.453/21.123ms; gas invalid 0, max pressure 174.673. First six-run attempt had one frame8 carve action spike 36.126ms, and repeat passed; follow-up ct-90z8 tracks action-frame tail. Carved minimum gas temperature 0.0400K warrants separate trace ct-ixii without arbitrary clamp.
