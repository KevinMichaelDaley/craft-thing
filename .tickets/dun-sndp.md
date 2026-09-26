---
id: dun-sndp
status: open
deps: [dun-sbk9, dun-rwls]
links: []
created: 2026-09-26T07:43:36Z
type: task
priority: 2
assignee: kmd
parent: dun-i1zd
tags: [gpu, wood, rot, streaming]
---
# Persist partial-immersion wood rot over months

Long exposure to the air-water interface gradually rots wood rigid bodies.

## Design

Accumulate fixed-point partial-immersion exposure using a simulation-time clock; specify wet/dry hysteresis, fully submerged and fully dry controls, rot thresholds, density/strength changes, and chunk/world-space persistence. Accelerated test clock must preserve deterministic results.

## Acceptance Criteria

After simulated months partially immersed wood changes rot state and mechanical behavior; short exposure, dry, and fully submerged controls follow defined rules; unloading/reloading preserves exposure exactly.

