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
