---
id: ctlo-6tk8
status: open
deps: []
links: []
created: 2026-10-23T14:39:26Z
type: task
priority: 1
parent: ctr-ebl5
tags: [vulkan, fluid, performance]
---
# Skip empty rigid-water displacement lines with parallel GPU classification

Displayed Wayland lit-water profiler identifies fluid phase0 10-16ms. Cooperative rigid-water displacement still traverses every coordinate serially, even where no body overlaps excess water. Classify immutable occupied overcapacity source candidates in parallel, preserve exact source and recipient order, and skip absent sources without CPU pixel scans or physics cadence changes.

## Acceptance Criteria

RED bounded-source-work regression plus exact canonical mass, pigment and left/right/upward displacement reference parity, including unknown residency and beyond-cache lines; GREEN core+sync validation, full suite, actual Wayland benchmark/capture. Parent total mean and worst <30ms target remains unchanged and must stay in progress if unmet.

