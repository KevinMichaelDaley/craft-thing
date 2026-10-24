---
id: dun-sndp
status: open
deps: [dun-sbk9, dun-rwls]
links: [ctlo-2q4f]
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


## Notes

**2026-10-24T22:07:29Z**

Reactive-state ownership/persistence is planned in ctlo-2q4f. Share wetness/exposure storage and explicit clocks with rot rather than repacking leaf-color/moisture flags or adding a second incompatible wood exposure record.
