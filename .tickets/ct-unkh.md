---
id: ct-unkh
status: open
deps: [ct-ortp]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Chew real tree material into independently owned wood inventory

Add extensible inventory/item interfaces and a chewing interaction against actual nearby tree wood. Choose and document an available provisional chew input; preserve requested A/D/Space/Q/E bindings.

## Acceptance Criteria

Use the supplied assets/player/beaver/beaver_chomp.ase (eight frames, user numbering is one-based): frame 1 idle; frames 2,3,4 enter chewing; frames 5,6,7 loop while chewing wood; frame 8 recovers to idle after release or invalid/exhausted target. Enter/recovery frames do not restart on every held input tick. Chewing gradually removes a localized amount of reachable tree wood with accumulated work proportional to actual occupied wood amount and material density; denser or more wood takes longer at the same chewing effort. Use shared physical density/quantity contracts rather than an invented RPG rule. Carry partial progress across simulation steps and give equivalent results for equal elapsed effort across frame cadences. Revalidate target material, reach, ownership and availability at mutation time. Removing supports must trigger the existing structural connectivity/release/fracture pipeline so newly unsupported wood becomes actual rigid bodies, preserving shape, mass, materials, appearance and reactive state; do not delete the whole tree, convert detached wood into inventory, or spawn decorative rigid substitutes. Only actually consumed wood is atomically credited to the responsible player inventory. No duplication or loss through retries, multiple players, missing chunks, failed body admission, or pending fracture transactions. Reject out-of-range/non-tree/empty targets without rewards. Inventory and item definitions remain extensible with no assumed RPG rules. Regression tests cover phased animation, density/amount/time dependence, incremental cutting, supported versus unsupported remnants, transactional rigid release and mass/ownership conservation. Demonstrate gradual tree cutting and falling detached wood in the actual application.


## Notes

**2026-11-01T23:45:12Z**

Continuation inspected the expanded gradual-cutting acceptance; no wood-removal or inventory implementation is claimed. Next RED coverage must include physical wood/dense-wood density and occupied cell amount, carried effort and cadence equivalence, localized cutting, and transactional unsupported release on missing chunks/body admission failure/concurrent retries. Shared density is include/dungeoncraft/material_density.h and fr_reactive_profile; real reactive species/parcel inventories already exist. Existing brush path dc_gpu_paint_material retries carving but is not an inventory transaction: body_carve/pixel_carve_parallel currently erase removed reactive handles. Do not credit inventory around brush success or count detached remainder as consumed. Extend the real reservation/publication gate to consumed parcel transfer plus connectivity release, retaining all surviving shapes/colors/materials/reactive lineage. Phased authored animation is implemented separately and mutates no wood.
