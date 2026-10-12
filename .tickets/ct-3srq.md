---
id: ct-3srq
status: open
deps: []
links: []
created: 2026-10-12T19:47:59Z
type: task
priority: 3
tags: [gpu, structural, mpm]
---
# Reclaim consumed MPM source records after all descendant parcels disappear

Consumed MPM roots conservatively retain a body bookkeeping slot and generation. Add GPU-safe reclamation only after no resident or checkpointed/cold descendant parcels remain; preserve transaction identity through save/reload.

## Acceptance Criteria

No early identity reuse; reclaim slots after complete parcel retirement including cold chunks and restart; saturated pools defer conservatively.

