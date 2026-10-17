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


## Notes

**2026-10-17T18:05:52Z**

Water now uses a separate atlas-indexed linear RGB concentration buffer (global descriptor32), seeded by imported water and advected/mixed by exact fluid flux including displacement/correction. Extend appearance persistence to serialize this pigment across save/reopen and chunk eviction; current chunk upload clears color state. Also retain pigment in bound MPM moisture and recover it with released pore water.

**2026-10-17T19:15:10Z**

ctr-nxdp implements linear RGB persistence for terrain, sparse body rest pixels and advected water in version 6 snapshots/chunks, plus GPU extraction/relocation/fracture transfer. Remaining scope includes full surface channels, emission, stable provenance and bound-particle moisture appearance.
