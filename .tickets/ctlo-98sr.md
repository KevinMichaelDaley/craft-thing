---
id: ctlo-98sr
status: closed
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

**2026-11-28T04:09:47Z**

Compiled reflection passed all five cases including readonly restricted directory/geometry, explicit index writes and writable records. The complete suite verifies dynamic growth, all five concave raster planes and typed contact outputs. The ordinary post-declaration benchmark documented in tools/diagnostics/README.md measured 32.414/38.887 ms mean/worst for 500 real-time displayed frames.
