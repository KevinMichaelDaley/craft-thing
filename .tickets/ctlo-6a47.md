---
id: ctlo-6a47
status: in_progress
deps: []
links: []
created: 2026-10-24T05:13:57Z
type: task
priority: 1
---
# Declare GPU body-root access readonly and nonaliasing for consumers

## Acceptance Criteria

Consumer SPIR-V declares the addressed body root readonly and restricted; only the explicit body-index writer retains write access. Preserve dynamic growth, descriptor publication and all GPU results; measure displayed performance before attributing a benefit.


## Notes

**2026-10-24T05:20:03Z**

A global readonly/restrict declaration exposed pathological cold compilation in structural.comp phase4 (CPU>2min,1.6GiB before meaningful GPU work; bounded init probe reproduces phase4 timeout). Restrict the declaration experiment to the five tested hot consumers (solver,raster,brush,MPM,scene). Structural kernels retain the conservative original declaration; the explicit index kernel remains the only actual GPU root writer.

**2026-10-24T05:22:32Z**

Selected five-consumer contract passes2/2; bounded engine init gets through all pipelines after restoring structural declarations (probe intentionally forbids the final initialization submit). Displayed real-time repeated brush is34.588ms mean /44.397ms worst over500frames, still failing. No demonstrated performance gain is claimed.

**2026-10-24T05:23:38Z**

Selected declaration final focused checks: dynamic runtime5/5, solver contact-state comparisons4/4, brush12/12, SPIR-V contract2/2. This retains GPU growth/retries and exact colors/physics. No full-frame target achievement or isolated speedup is claimed.
