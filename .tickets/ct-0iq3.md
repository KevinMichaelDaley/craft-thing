---
id: ct-0iq3
status: in_progress
deps: [ct-il9b, ct-i5a4]
links: [dun-spbq]
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Timed support failure and atomic structural-to-rigid conversion

Accumulate material-dependent overload damage and confirmed support-loss time in simulation seconds. On failure, cut failed joints, relabel islands and atomically reserve/commit rigid outputs while keeping rooted islands structural. Reuse common geometry/material-volume conversion contracts with dun-spbq without making dry structural support depend on unfinished water behavior.

## Acceptance Criteria

Brief impact or temporary support loss below the configured grace does not trigger topology failure; sustained overload, anchor removal or an actual mechanism does. Different frame cadences produce equal failure time. Missing chunks and numerical UNKNOWN do not age failure. A sound unsupported island becomes a freely falling rigid assembly; sound connections remain bonded. Geometry, material volume, mass and linear/angular momentum are preserved within documented tolerances; concavities remain empty. Full output pools roll back conversion without ghost copies, deleted supports or lost mass.

