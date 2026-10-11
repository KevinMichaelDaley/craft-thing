---
id: ct-1n9n
status: open
deps: [ct-0iq3]
links: []
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Persist structural graph identity, damage and conversion state across streaming

Persist structure/member/joint IDs, world anchors and footprint cells, rest geometry, attachment types, material state, grain orientation, damage, grace timers and output transaction identity. Derive transient matrices/labels on reload; checkpoint dirty graph mutations in the world save transaction.

## Acceptance Criteria

Camera rebases leave forces, stress and countdowns invariant. Cold neighbor constraints remain UNKNOWN until loaded. Save/reopen mid-overload and across a cut preserves exact authoritative state; no duplicated descendants, lost joints or reset timers. Page-slot reuse cannot anchor to unrelated terrain. Saturated pending conversions survive restart and later commit exactly once. Older worlds without structural records load unchanged.

