---
id: ctpe-847v
status: open
deps: [ctpe-2lx4]
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

