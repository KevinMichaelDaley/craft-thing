---
id: ctlo-53vm
status: in_progress
deps: []
links: []
created: 2026-10-24T07:15:19Z
type: task
priority: 1
---
# Resolve solver body state directly from its known slot

## Acceptance Criteria

Known body-state reads and corrections resolve slab/word offsets directly, avoiding generic tagged address decoding. Keep legacy and runtime layouts exact, including overflow flags and growth. Profile zero generic body-state resolutions and compare frozen solver body bytes; measure displayed frames.

