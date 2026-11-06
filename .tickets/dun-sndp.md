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

**2026-11-06T04:37:49Z**

ABI-8 rot wave: the reactive state grows a 29th word (rot_exposure, partial-immersion rot accumulator), ABI 7 to 8, arena entries 36 to 37 words, chunk attachment stream 35 to 36, arena request 38 to 39 (all stride constants derived from the layout headers). fr_wood_rot_reference accumulates exposure only at the air-water interface (wet fraction 0.05 to 0.95 - the partial-immersion window); each full unit converts fuel to inert rot residue mass-conservingly, so the rotted body's draft and mass change. Dry and fully saturated parcels pause. Wood rot tests 6/0 (month-stage conversion, dry/saturated controls, gradual accumulation, mass conservation, transactional rejection). Snapshot: old ABI-7 packets are rejected by the explicit header gate (acceptable pre-release). ALSO FIXED en route: the thermal compute pipeline declared a 32-byte push while the layout grew to 9 words - silent clipping on wide worlds. REMAINING for this ticket: (1) the underwater pace contrast recalibration - with the thermal pass live the cold pool cools the burning film, narrowing the greek/oil late-window pace to 1.17x; the design intent (greek roars as it soaks) needs the roar coupled to soak depth or the oil throttle verified at full saturation, (2) the siege-level reload probe (the chunk export rejects body-parcel chunks; the record snapshot path is proven), (3) the GPU rot accumulation pass (the reference and tests are done; the GPU fold into body_wet_parcel is not yet written).
