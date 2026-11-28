---
id: ctlo-4fu2
status: closed
deps: []
links: []
created: 2026-10-24T06:46:01Z
type: task
priority: 1
---
# Cache exact pressure stencil metadata across SOR sweeps

## Acceptance Criteria

Cache terrain and face coefficients per pressure batch while preserving divergence evaluation; retain every red/black sweep, refresh after between-batch edits, and match dense reference pressure/velocity bytes. Validate displayed full-frame timing and memory budget.


## Notes

**2026-10-24T06:54:39Z**

Cache stores four terrain/face weights and their exact count, refreshed each pressure batch after velocity prediction and after edits. Pre-storing divergence changed floating-point contraction on mixed partial faces; removed that part so divergence retains original evaluation. Four focused tests now pass, including bytewise pressure, velocity and cell comparisons on8192 mixed cells across all six batches and an inserted-water edit. Separate stencil-writer SPV keeps sweep metadata readonly. Ordinary displayed benchmark underway.

**2026-11-28T03:57:27Z**

Verified projection.comp stores the four face weights with open/wet flags in the device-only cache and evaluates divergence inline. Eight pressure-work tests passed on the RTX A2000, including mixed faces and edits between batches against dense bytes. Cached and dense mapped allocations both measured 72,737,632 bytes. The displayed 1280x720 production runs measured dense p95 12.978 ms and concave p95 22.973 ms. The separately proposed divergence-payload cache is tracked by ctlo-udlu.
