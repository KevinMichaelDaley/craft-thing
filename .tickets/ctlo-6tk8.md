---
id: ctlo-6tk8
status: closed
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


## Notes

**2026-10-23T14:46:54Z**

RED profile measured11371 visits versus3277 excess-water candidates; GREEN parallel immutable source masks visit exactly candidates, zero visits for no-body lines. Exact reference/mass parity6/6 profile,4/4 standard,4/4 width2113,4/4 height2113 and water-color4/4 core+sync; dependencies3/3. Actual Wayland lit-water60frames mean74.680/worst90.725ms versus prior86.481/102.980, four paused captures identical. Parent <30ms remains unmet; full-suite checkpoint follows.
