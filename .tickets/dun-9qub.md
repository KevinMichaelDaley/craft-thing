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


## Notes

**2026-10-05T05:13:34Z**

dun-ci2x now supplies GPU-owned contacts through binding 4. dc_gpu_contact_word_offset locates the header after all broadphase records; header is 12 words, contact records are 128 bytes. Six public counters precede an indirect command at words 6..8. Consumers must gate ALL corrections on nonzero contact overflow (capacity=1, world=2, incomplete broadphase=4). Normal points B-to-A; body pairs have sorted IDs, terrain/boundary/MPM have body_b=0. Anchors are canonical signed world chunk plus local 16.16; feature_chunk identifies a terrain/MPM cell independently of where its witness lies. Material namespaces differ for bodies and cells. High-bit body feature denotes an edge. Surface coefficients are prototypes; derive physical mass/inertia and density from material properties. Preserve previous poses and use bounded substeps for fast/thin bodies because narrowphase is discrete. The existing timestamp includes contact generation; no ordinary-frame counter/transform readback.
