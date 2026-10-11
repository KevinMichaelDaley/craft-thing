---
id: ct-il9b
status: open
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

