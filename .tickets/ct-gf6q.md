---
id: ct-gf6q
status: in_progress
deps: [ct-0iq3]
links: [dun-75oh]
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Material-dependent structural fracture, splitting and crumbling

Use tensile/compressive/shear stress, buckling, accumulated damage and fracture energy to choose failure locations and outputs. Strong unsupported material falls intact when its joints remain sound. Overstressed stone cracks to bounded rigid chunks; severe crushing and weak dirt/mortar-like material produce MPM debris; wood splits along its grain with retained coherent pieces. Share transaction infrastructure with the impact-fracture path.

## Acceptance Criteria

Same graph/load with distinct material profiles gives distinct failure mode/time. Detached sound wood does not automatically turn to gravel; stone in tension cracks before equivalent compression; weak cohesive material crumbles; broken bracing can cascade after load redistribution. A dirt/mortar profile does not silently redefine ordinary dirt terrain. Mass, composition, water carried by debris and momentum are accounted; fracture dissipates bounded energy and creates no unexplained launch impulses. Every rigid/MPM allocation failure is transactional.


## Notes

**2026-10-11T23:26:00Z**

Material identity and initial GPU mortar crumble implemented: distinct density 1.6, exact clipped cell volume/centroid, cumulative fixed-point mass, all-destination preflight, rigid ownership disabled only on commit, fracture-work budget check, no virtual launch. Two targeted tests pass; full headless 196/196. Wood/stone fragmentation, saturation retry, rotated/seam cases, full energy accounting and actual MPM evolution remain unfinished; ticket remains in progress.

**2026-10-11T23:37:20Z**

Four material regressions now pass headless and quarter-native; full battery 198/198. Rotated seam crumble: 41 particles, mass 38.399994 and original centroid; E=20494.142578 accounted as fracture work14400 plus remaining dissipation. Corrected physical backward-error scale to retain absolute modal terms before cancellation; same tolerance/rank thresholds, all 17 equilibrium tests including UNKNOWN safeguards pass. Remaining work includes pool rollback/retry, actual MPM evolution, wood/stone GPU-born fragments and shared impact behavior.

**2026-10-11T23:45:45Z**

Seven material cases pass in normal and quarter-native configurations; full battery 201/201. Saturated destinations preserve the stationary source, mass and particle pool, then retry exactly once after freeing slots. Eight actual MPM ticks keep mass 38.399994 and move mean y to 47.77. Material density constants now shared across C profiles, bodies, MPM and paint. Warm static quarter budget passes: 8.760 ms rigid/structure, 18.071 ms whole, allocations 64.445/107.196 MiB. Wood/stone rigid fragments, shared impact path and persistent output identity remain unfinished.

**2026-10-12T15:55:48Z**

Stone rigid birth checkpoint: nine material tests pass normal and quarter-native; full headless 204/204. GPU clipping reserves body slots and output identities atomically, preserves mass/inertia and actual momentum without virtual launch, and rerasterizes through GPU indirect dispatch. Five specialized structural shader phases avoid reproduced device loss from the combined shader. Warm quarter budget 9.110 ms rigid/structure, 18.337 ms whole. Wood splitting, shared impact path and persistence remain unfinished.

**2026-10-12T16:01:08Z**

Wood grain split implemented and validated under actual rigid-body contact load. Coherent children retain wood material and combined mass14.4; grain-direction fracture work is bounded by recovered energy. Ten material cases pass normal and quarter-native; full headless205/205. Severe stone crushing and impact/persistence remain unfinished.

**2026-10-12T16:08:35Z**

Severe axial stone compression now selects the atomic clipped gravel transaction (material5, density2.7) instead of tensile rigid splitting. Hard-contact column regression preserves all5.4 mass; eleven material cases pass normal and quarter-native, full headless206/206. Impact sharing, composition/water and persistence still remain.
