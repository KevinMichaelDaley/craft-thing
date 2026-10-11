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

