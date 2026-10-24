---
id: ctlo-vkrr
status: in_progress
deps: []
links: []
created: 2026-10-24T18:39:07Z
type: task
priority: 1
---
# Enable the measured Vulkan performance profile by default

## Acceptance Criteria

Fresh default contexts use two-substep particle-buffer alternation, specialized/shared MPM kernels, fused grid work, safe apply overlap and zero-reduction skipping; experimental boundary caching remains opt-in with no unused allocation. Explicit reference overrides remain available. Actual displayed saved-scene repeated-cut benchmark must meet full-frame30ms budget without environment-only configuration.


## Notes

**2026-10-24T18:57:17Z**

GREEN factory tests2/2 under actual core+synchronization validation, coupled alternating-pool test remains byte-exact with defaults, and pipeline-contract tests4/4. Structural SOLVE/IMPULSE variants deliberately retain zero-load capture because it records support dependencies even when the impulse is zero. Default context enables the measured profile and leaves boundary caching opt-in with zero scratch allocation when off. First default500-frame replay23.536ms mean31.217ms worst: average improved, strict worst-frame gate still requires verification/work.
