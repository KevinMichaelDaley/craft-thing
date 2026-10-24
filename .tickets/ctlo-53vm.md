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


## Notes

**2026-10-24T07:21:28Z**

RED generic body-state resolutions22800, exact reference bytes but fails zero-decode bound. GREEN0 while contact resolutions remain0 and capacities stay2/invocation, body bytes match frozen reference after dynamic queue growth under actual sync validation1/1. Nonprofiled SOLVE4407 instructions,290 sends, zero spills (debug metadata stripped only to fit existing compiler-probe input buffer). Displayed performance pending.
