---
id: ctlo-70tf
status: in_progress
deps: []
links: []
created: 2026-10-24T07:53:42Z
type: bug
priority: 1
---
# Keep pressure stencil cache within mapped and device memory budgets

## Acceptance Criteria

Pressure coefficient cache consumes no mapped allocation relative to the dense reference; retain existing mapped and device budget thresholds. Pack four exact aperture values with wet/open flags in device-only storage. Preserve bytewise pressure/velocity/cell reference parity and cache lifecycle.


## Notes

**2026-10-24T07:57:52Z**

RED: cached128x64 solver maps62382764 bytes versus62218924 for dense reference; focused test fails zero additional mapped-memory requirement. Full native UI19/1 and quarter172/1 each fail the unchanged mapped-memory assertion; wet scene89.026MiB mapped and107.196MiB device. All other native checks pass.
