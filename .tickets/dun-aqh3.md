---
id: dun-aqh3
status: closed
deps: []
links: []
created: 2026-09-29T10:14:52Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, rendering]
---
# Reduce speckled breakup in fast falling water streams

The high-paint capture now has no regular horizontal bands, but the falling stream remains visibly mottled and breaks into small isolated pixels. Investigate transport/marker coherence while preserving Eulerian mass conservation and no per-frame readback.

## Acceptance Criteria

A held brush produces a visually coherent falling stream in the 64x128 before/after capture, with a quantitative connectedness or coverage assertion; existing river, seam, and performance checks pass.


## Notes

**2026-09-29T20:56:02Z**

Regression at 45 held-paint frames: largest four-connected stream grew from 174/326 to 240/286 wet cells in x24..40,y16..55. Fast unsupported water >16 cells above solid no longer receives hydrostatic head; supported pools and seam retain it. Full make test passes. 960x540, 64-chunk benchmark: 84.44 tick Hz versus prior 85.01 Hz. Residual lateral midair fan tracked in dun-3qbo.
