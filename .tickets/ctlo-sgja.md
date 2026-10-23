---
id: ctlo-sgja
status: in_progress
deps: []
links: [ctlo-inhv]
created: 2026-10-23T15:05:14Z
type: bug
priority: 1
tags: [physics, terrain, regression]
---
# Cut terrain remains suspended when the legacy rigid-body pool is full

User reports cut stone and soil remain suspended in the level03 demo. Read-only inspection of world_chunks_level_piece03_equilibrium/rigid_bodies.bin found all 64 body records active. Gameplay still uses DC_GPU_BODY_CAPACITY=64 and rejects births when full. Existing dynamic-storage prototypes on main (ct-hhmy, ct-q6fr, ct-f83a) have not migrated gameplay consumers. This is the same unresolved runtime capacity limitation, not a demonstrated lighting regression. Preserve the user's saved bodies and terrain; do not increase a fixed ceiling or delete bodies to mask it.

## Acceptance Criteria

Finish dynamic runtime body storage and all consumers, including extraction, solver, contacts, renderer, geometry, resource queues and checkpoints. An edited isolated mixed-material component must acquire a body and fall when more than64 existing bodies remain alive; repeat growth and retain IDs, exact geometry, mass and colors. Preserve GPU physics and user data. No arbitrary larger compile-time maximum.

## Investigation

Read-only snapshot header: DCS1 version6, body_words8704, graph_words40000, flags5, 70 saved chunks, 2688 pixel words. All64 body records have active=1. A temporary mixed stone/soil cut reproduction with90 unchanged loaded islands passes (both pieces extract/fall and unchanged terrain stays fixed), as do all7 existing extraction tests; the suspected batched bond-evidence loss was not reproduced, so no implementation was changed for that hypothesis. Removed the temporary probe after diagnosis. The saved world was not altered by this investigation. Runtime migration remains unfinished; no fix is claimed.


## Notes

**2026-10-23T15:24:25Z**

Resumed implementation after user reiterated no fixed limit. Merged main storage groundwork into current gameplay branch (6730155), retaining renderer/physics optimizations and opt-in lighting. Re-ran actual material sandbox RED:90 requested live bodies yields64, exact mass/dynamic64; failure captured in build/runtime_growth_red.log. Migrating shared body/data accessors before enabling addressed gameplay storage; runtime growth is not yet fixed.

**2026-10-23T16:45:33Z**

Addressed gameplay migration now passes actual Wayland sandbox with90 bodies and repeated64->128->256->512 growth for320 bodies; v7 counted checkpoint reload retains all320 IDs and exact stone masses under core+sync validation (build/runtime_counted_growth.log). Production default now addressed; geometry/colors and queue resource growth plus fracture/leaf journal migration are implemented and undergoing focused validation. Ticket remains in progress until complete regressions and original-world graphical replay pass.
