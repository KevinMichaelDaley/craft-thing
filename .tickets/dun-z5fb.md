---
id: dun-z5fb
status: closed
deps: []
links: [dun-4w8r]
created: 2026-09-29T03:09:11Z
type: bug
priority: 0
assignee: kmd
---
# Let water activate cold offscreen chunks and spread beyond a stationary viewport

## Acceptance Criteria

A large water pour propagates beyond the resident GPU view without camera movement, continues across offscreen workspace seams toward equilibrium, and does not require per-frame GPU readback.


## Notes

**2026-09-29T03:33:09Z**

Fixed the reported stationary-camera horizontal edge with cold offscreen prefetch, outward workspace placement, and GPU workspace seam exchange. Regression covers the first cold chunk and the next GPU workspace. General GPU-driven wet-frontier activation across arbitrary rows/axes is tracked by dun-4w8r.
