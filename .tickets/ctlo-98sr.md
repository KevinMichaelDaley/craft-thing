---
id: ctlo-98sr
status: in_progress
deps: []
links: []
created: 2026-10-24T06:42:58Z
type: task
priority: 1
---
# Preserve immutable body-directory and geometry access in GPU consumers

## Acceptance Criteria

Selected root consumers declare the immutable directory and geometry readonly without constraining the explicit index/free-mask writer or fracture geometry writers. Exact dynamic body, raster and contact tests pass; measure ordinary displayed performance.


## Notes

**2026-10-24T06:46:00Z**

Readonly directory/geometry consumers pass4 SPIR-V contract cases, exact raster five-plane comparisons, and dynamic runtime5/5. MPM does not dereference these views and is excluded from the directory-specific test. Index/free-mask writer remains writable.
