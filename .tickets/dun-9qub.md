---
id: dun-9qub
status: open
deps: [dun-ci2x]
links: [dun-mi1e]
created: 2026-09-26T07:43:36Z
type: task
priority: 2
assignee: kmd
parent: dun-5kye
tags: [gpu, rigid, xpbd]
---
# Solve rigid contacts with parallel Jacobi XPBD on GPU

Apply terrain and body contacts using parallel Jacobi XPBD iterations.

## Design

Accumulate positional and rotational corrections in separate per-body buffers, then apply simultaneously each iteration; use compliance, mass/inertia, friction, restitution, warm-start policy, and bounded substeps. Update velocity from corrected poses and publish final occupancy before fluid/MPM.

## Acceptance Criteria

A multi-body stack settles without race-dependent penetration or energy blow-up; moving support and hard drops behave reproducibly; GPU stage order and timing are validated, with no per-frame CPU contact solve or readback.

