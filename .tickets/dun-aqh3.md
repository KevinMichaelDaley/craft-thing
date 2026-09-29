---
id: dun-aqh3
status: open
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

