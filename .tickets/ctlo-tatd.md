---
id: ctlo-tatd
status: in_progress
deps: []
links: []
created: 2026-10-24T05:24:10Z
type: task
priority: 1
---
# Expose cooperative transport experiments for displayed comparisons

## Acceptance Criteria

Transport long-run scan and cached-page variants are opt-in pipeline specializations, defaulting off, without affecting other fluid kernels. Verify specialization wiring and existing exact mass/pigment equivalence tests; measure displayed performance before enabling defaults.


## Notes

**2026-10-24T05:31:16Z**

Specialization wiring/isolation4/4 passes with fake Vulkan pipeline creation. Existing cooperative transport fixture passes all fourteen variants in four directions with exact mass and pigment, including stale/unknown pages, partial faces, long travel and stagnant runs. Starting equal-work displayed comparisons; defaults remain off.

**2026-10-24T05:34:37Z**

Equal-work displayed300-frame means: reference29.434ms, long29.194ms, page29.539ms, combined29.692ms. All worst frames exceed30ms; no reliable speedup established and both defaults remain off. Exact transport and specialization tests pass.
