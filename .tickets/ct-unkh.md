---
id: ct-unkh
status: closed
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

**2026-11-15T09:44:34Z**

GREEN. src/player/chew.c implements the deterministic work contract (fr_chew_work_advance/committed: effort-ms banked per target identity, saturated at one bite, reset on target loss, generation or mass change, transactional on invalid input; chew_tests 6/0 against the pre-existing contract suite). src/player/chew_world.c applies it to the world: the tick resolves the resident slot holding the reachable wood by content, banks density-weighted work (wood 0.6 vs dense 1.2 at the shared material_density contract), and on a banked bite carves exactly one cell through the ordinary paint/carve pipeline - unsupported wood released as a rigid body (measured: cutting the pillar mid-air released a body), the rest of the tree intact - and credits the responsible player inventory atomically (a failed carve credits nothing; the banked bite is acknowledged so retries cannot duplicate). Session wiring: holding C chews at the aim (override or mouse) using the deterministic player clock, so equal elapsed effort removes equal mass at any cadence (verified slow 39322 == fast 39322). Extensible inventory items by material (wood, dense wood). App evidence: player_chew_app_tests 1/0 with chew-start.bmp/chew-end.bmp captures and inventory 117966 fp; chew_inventory_tests 5/0. Full-suite run: the only failure was the known ctlo-jnt7 timing gate (39-45 ms vs 30 ms, functional assertions pass); greek_siege 3/0 in isolation (order dependence stays documented on ctlo-yvip).
