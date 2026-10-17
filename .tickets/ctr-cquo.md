---
id: ctr-cquo
status: closed
deps: []
links: []
created: 2026-10-17T17:57:00Z
type: bug
priority: 1
---
# Reduce soil hydraulic conductivity

## Acceptance Criteria

Soil throttles flow strongly relative to open water, with conservative mass and cached/uncached flow agreement.


## Notes

**2026-10-17T18:07:13Z**

RED: soil received32767 units, identical to open flow. GREEN: same drive receives511 units (1/64 face conductance) with exact mass conservation. Conductance is applied to both sides of each face and to prefix/tree and uncached aperture queries; existing exact transport10/10, displacement4/4, aperture3/3 pass with core+sync validation.

**2026-10-17T18:17:06Z**

Final full suite conserves fluid mass and passes existing31 fluid tests plus exact cached/uncached transport and aperture fixtures. Actual scene smoke12 ticks passes with new soil conductivity. Open-water transport remains unchanged; soil face conductance is an explicit1/64 tuning value.
