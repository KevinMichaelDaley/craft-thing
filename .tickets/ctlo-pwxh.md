---
id: ctlo-pwxh
status: in_progress
deps: []
links: [ctlo-jnt7]
created: 2026-10-24T03:13:46Z
type: task
priority: 1
---
# Cache pixel quad geometry during raster and hydro clipping

## Acceptance Criteria

GPU clipping matches the frozen scalar oracle bit for bit for sparse and concave mixed-material pixel shapes, rotated poses, full and partial fluid cells, and face intersections. At most six geometry reads per pixel clip or face query. Verify actual displayed frame performance without changing physical resolution, iterations, geometry or lighting defaults.

