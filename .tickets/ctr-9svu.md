---
id: ctr-9svu
status: closed
deps: []
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Establish renderer appearance sidecars and stable texel-to-material ownership

Add renderer-owned albedo/appearance storage and APIs with explicit terrain-world and body-rest texel ownership. Do not alter dc_cell_t, mass or physical material meaning; account for different terrain/body material enum namespaces. Support optional higher visual texel resolution through an owner-index mapping.

## Acceptance Criteria

Distinct colors on same material, mixed-material ownership and holes, world/rest transforms, slot-generation protection, deterministic old-world fallback and physics invariance are tested. Document formats, memory and API/storage contracts.


## Notes

**2026-10-14T03:19:08Z**

Six observed RED-to-GREEN CPU contract tests pass: mixed material holes, visual-scale mapping, material namespaces, explicit sidecar color overrides, generation recycling and inverse rest transform. Reference/import API uses fixed caller storage; GPU generation/transfer will use renderer buffers. Full headless refactor validation waits for coordinated GPU window; do not mark ticket complete yet.

**2026-10-14T04:12:26Z**

Full make test completed exit0 with Vulkan validation on the Intel device. Ownership6, texture reference4+GPU1, occlusion1, directional-light7, shading3 checks all pass; physical headless battery passed unchanged. Sidecar API/storage contract complete; renderer frame wiring, source transfer/persistence and visual acceptance remain downstream tickets.
