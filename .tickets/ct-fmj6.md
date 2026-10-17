---
id: ct-fmj6
status: in_progress
deps: []
links: []
created: 2026-10-17T03:42:04Z
type: bug
priority: 1
tags: [vulkan, physics, correctness]
---
# Make GPU MPM convergence control independent of lane scheduling

MPM component mode5 currently lets every workgroup lane nonatomically read the changed flag, overwrite the indirect work count and clear the flag. A later lane can observe the cleared flag and prematurely stop component convergence. This is a source-audit finding awaiting a controlled GPU RED; no display-crash cause is claimed.

## Acceptance Criteria

A compiling controlled scheduling test derived from the production shader must fail before the fix and pass afterward. Exactly one invocation publishes convergence control. Preserve all indirect triplet bounds and untouched words. Existing winding dirt, cohesion, mass, full headless and displayed concave regressions pass with Vulkan validation. Measure native360 frame effects without weakening the existing mean or p95 gates.


## Notes

**2026-10-17T03:42:10Z**

Controlled schedule source generator and GPU test compile and SPIR-V validate. GPU run is queued behind current full regression; production mpm_component shader remains unchanged until failing RED is observed.

**2026-10-17T03:43:36Z**

Controlled production-source schedule test fails as expected: later lanes overwrite the correct7-item convergence count with0, noVUIDs. Full checkpoint0766758 headless regression separately completed372reportedpasses,0failures,0VUIDs before this fix.
