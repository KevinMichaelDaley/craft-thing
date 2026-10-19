---
id: ctpe-847v
status: open
deps: [ctpe-2lx4, ct-wuci]
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Instantiate imported props as terrain, structures and exact rigid bodies

Integrate validated asset placement with GPU physics/extraction/structural interfaces. Preserve object identities, four-connected exact holes/mixed material geometry and authored support hints that must contact real terrain. Connect appearance ownership atomically through coordinated renderer/extraction hooks.

## Acceptance Criteria

Concave >256px/mixed bodies, disconnected IDs, actual terrain roots/automatic detachment and graph-based structures are tested. Capacity/overlap/UNKNOWN failure rolls back physics and visual ownership; density/buoyancy material-derived. No CPU runtime component solve or stale serial extraction.


## Notes

**2026-10-14T03:45:45Z**

Atomic multi-role GPU authoring entrypoint requires completed ct-wuci parallel extraction/publication hooks plus geometry/body/graph/appearance joint reservation. Exact row-major material/role/anchor/object maps and separate 64-byte appearance payload documented in design/prop_package_v1.md. Parent approved explicit dependency. Do not bolt a serial authoring birth path into stale extraction shaders; placement remains open until actual GPU API and rollback verified.

**2026-10-17T09:27:19Z**

Renderer combined branch now implements terrain-only GPU package placement for actuallevel_piece01: full rollback before any pixel publishes on unknown destination/body overlap/unsupportedrole; canonical wood/stone/dirt/water, particles/fluid and resident appearance installed together. Source transparency skips world cells. General rigid/structural role admission remains unfinished and ticket staysopen. Actual displayed centralchunk brush/physics test passes with synchronization validation.
