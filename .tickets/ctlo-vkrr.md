---
id: ctlo-vkrr
status: closed
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

**2026-10-24T19:13:37Z**

Default Wayland level03 repeated-cut500 replay after restoring inline divergence passes23.976ms mean29.559ms worst. Longer1000 repeat24.408ms mean30.167ms worst still fails strict gate. Four paused captures identical in both. Continuing active-cut optimization; resolution remains1:1 at480x270.

**2026-10-24T19:26:00Z**

Final default profile adds cooperative staged color copies and fused visible albedo refresh/shading. Two consecutive actual Wayland saved-scene1000-frame repeated-cut replays pass:24.079/29.334ms and24.133/29.147ms mean/worst, four paused captures identical. Physics/render480x2701:1, full solver cadence/substeps retained, no tuning environment overrides, lighting off by existing default. Full regression rerun underway.

**2026-10-24T19:29:19Z**

Final make -j8 -i test completed:660 reported passing cases, only2 pre-existing authored-stage failures tracked in ctr-ojvy, zero compiler warnings. Fluid specialization1/1, buoyancy14/14, updated contact selector10/10, cooperative color probe1/1 and carving12/12, albedo2/2/full scene1/1 all pass. Actual Wayland UI and quarter-native suites next.
