---
id: ctsdb-yps2
status: in_progress
deps: []
links: [ct-yctv, ctisf-f0wy]
created: 2026-11-26T22:18:11Z
type: task
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, thermal, vulkan]
---
# Apply ice-storm cooling alongside wetting on the GPU

The storm waits on the GPU for every mapped texel and re-imports each cooled parcel through a cold checkpoint path. A failure after the water brush can leave partly cooled terrain.

## Acceptance Criteria

One bounded native Vulkan brush applies both effects with no parcel readback or re-import. Retain 30 percent of the surviving parcels’ full heat, preserve all other state and owner identity, and preserve the validated world footprint and existing water rules.


## Notes

**2026-11-26T22:20:16Z**

RED in the production ability path: an admitted cast invoked the ordinary host brush and read parcel state on the mapped page instead of using one chilled-water brush. Two dispatch-work regressions failed and all three rejection controls passed. The native regression compiled and asserts queue work, compensated heat, and byte-exact ownership, stock and off-disc state.
