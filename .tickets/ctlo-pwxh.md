---
id: ctlo-pwxh
status: closed
deps: []
links: [ctlo-jnt7]
created: 2026-10-24T03:13:46Z
type: task
priority: 1
---
# Cache pixel quad geometry during raster and hydro clipping

## Acceptance Criteria

GPU clipping matches the frozen scalar oracle bit for bit for sparse and concave mixed-material pixel shapes, rotated poses, full and partial fluid cells, and face intersections. At most six geometry reads per pixel clip or face query. Verify actual displayed frame performance without changing physical resolution, iterations, geometry or lighting defaults.


## Notes

**2026-10-24T03:15:18Z**

RED on Iris Xe: 13,824 clipping cases match the original scalar oracle exactly, but a volume query fetches geometry 16 times and a face query 64 times, failing the six-read bounds. The four-pattern fixture covers sparse and concave mixed materials, positive and negative rotation, full/partial fluid cells, and face intersections.

**2026-10-24T04:00:15Z**

GREEN: all 55,296 rotated sparse/concave mixed-material volume, centroid and face cases match the frozen scalar oracle bit for bit. Maximum geometry reads are five for both volume and face queries, compared with 16 and 64 before caching. Current complete-frame workload remains above its limit; an isolated graphical comparison is still needed before attributing a frame-time improvement to this kernel.

**2026-11-28T04:23:09Z**

On the RTX A2000 the restored bitwise assertion failed for 2202 of the first 13824 face cases, although all were within the existing tolerance. Caching the occupied cell once and retaining the ordered edge arithmetic now passes all 55296 volume, centroid, face and overlap checks with five geometry reads and zero bit mismatches. Frozen reference and numeric-tolerance checks remain intact. Displayed performance for this change is pending.

**2026-11-28T04:44:37Z**

Displayed quarter-native RTX check passed on the persistent Xephyr server: the stock two-body, sun and four-light renderer fixture completed 90 physics/render frames in 0.907s, with exact masses and four byte-identical paused frames. The separate 180-tick concave dynamics display also passed in 3.233s including fixture pacing and readbacks. Coupled with the 55,296 exact frozen-oracle cases and maximum five geometry fetches, this checks the implemented change at actual resolution and cadence. No isolated timing gain or saved-scene worst-frame result is inferred.
