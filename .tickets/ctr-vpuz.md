---
id: ctr-vpuz
status: closed
deps: []
links: []
created: 2026-10-19T00:33:57Z
type: task
priority: 1
---
# Enable integrated lit renderer and sun shadows in gameplay scenes

## Acceptance Criteria

Scene demo starts lit with sun and conservative shadows; F8 toggles lit/albedo without changing physics; paused frames are stable. Validate on Vulkan and retain authored appearance.


## Notes

**2026-10-19T01:21:42Z**

Integrated scene starts lit with sun; F8 preserves sources while toggling albedo. Real level03 UI regression passes with core+sync validation, unchanged paused frames, packed terrain and trunk support. Lighting now uses propagated direct/diagonal/diffuse fields. Performance limitation remains separately tracked under ctr-21eh.
