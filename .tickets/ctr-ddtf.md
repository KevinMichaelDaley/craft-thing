---
id: ctr-ddtf
status: open
deps: [ctr-9svu, ctr-8y4b, ct-wuci]
links: []
created: 2026-10-14T03:12:56Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Preserve appearance through parallel extraction, fracture and persistence

Coordinate ct-wuci hooks: transfer stable appearance/provenance atomically during GPU component birth and fracture, structural release and arena relocation. Persist authored overrides and generation metadata with safe versioned migration; retain survivor colors exactly.

## Acceptance Criteria

Birth/rollback/retry, mixed pixels, fracture, page/body reuse, off-viewport cached geometry, rebase/eviction/save/reopen preserve appearance and physical identity with no loss or duplication. GPU-sized work is parallel. Integrate completed ct-wuci changes; never restore its old serial extraction.

