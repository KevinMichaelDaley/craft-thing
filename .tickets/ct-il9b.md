---
id: ct-il9b
status: in_progress
deps: [ct-i5a4, ct-touk, dun-9qub]
links: []
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# GPU equilibrium and rigid-to-structure reaction loads

Solve displacement/rotation and stress for anchored graph components with bounded GPU work. Couple the rigid XPBD pass to work-conjugate structural forces and moments at actual contact points; include self-weight and avoid double counting impulses vs positional multipliers. Distinguish mechanical mechanisms from numerical nonconvergence.

## Acceptance Criteria

An unloaded bridge is stable; a centered rigid load splits reactions symmetrically; an eccentric load adds the correct moment and overload location; an angled brace improves a sideways cantilever through equilibrium. Action/reaction and force/moment budgets close. Unbraced hinges do not count as full support. Nonconvergence and overflow freeze damage rather than inventing collapse. Quarter-native timestamps, memory and residual/convergence sweeps are reported; no CPU runtime solve.


## Notes

**2026-10-11T18:29:16Z**

Implemented GPU-only bounded equilibrium, oriented finite interfaces, active unilateral/Coulomb bearings, actual work-conjugate XPBD loads, physical residual refinement and conservative geometric mechanism certification. Seventeen equilibrium tests pass normal/quarter-native; all 181 headless tests and full quarter-native/UI batteries pass with Vulkan validation. Full-bound chain is safely UNKNOWN when float conditioning fails; overflow/clipping/cold/weak numeric states freeze future damage. Warm 16-member timing 6.979 ms rigid/topology/equilibrium, 17.335 ms whole tick; 64.407 MiB mapped +107.196 MiB device. One earlier existing visual-test SIGSEGV remains unreproduced in full retry, standalone, GDB and ASAN/UBSAN; not represented as fixed.
