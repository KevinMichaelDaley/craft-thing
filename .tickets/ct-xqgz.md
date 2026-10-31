---
id: ct-xqgz
status: closed
deps: []
links: []
created: 2026-10-30T16:25:02Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, tests, gas]
---
# Gate large-CFL Vulkan gas tests on required denormal feature

On RTX A2000, gas_display_tests reports 32 passed/9 failed because Large-CFL policy requires shaderDenormPreserveFloat32 and this device does not advertise it. Practical gas path passes on this GPU. Test cases should query features and skip unsupported Large-CFL configurations with explicit coverage reports while still running practical transport and exact tests on capable devices.

## Acceptance Criteria

On NVIDIA without float32 denormal preservation, gas display/publication tests report capability-based skips rather than failures; on supporting devices, all existing Large-CFL assertions still run. Full gas suite passes on this machine.


## Notes

**2026-10-31T01:46:08Z**

On RTX A2000 lacking float32 denormal preservation, capability probe and feature-based RUN_LARGE/RUN_DENORM_COMPARISON keep Large-CFL and two comparisons of disabled compact practical paths as explicit skips. Under tools/smoke_profile, gas_display_tests 34 passed/0 failed/8 skipped; gas_publication_display_tests 67 passed/0 failed/10 skipped. On capable GPUs all original assertions still run. Full make test pending.

**2026-10-31T02:06:45Z**

Final verification after refactor: full tools/with-vulkan make -j8 test PASS; focused gas display34 passed/0 failed/8 skipped, publication67 passed/0 failed/10 skipped under smoke_profile on RTX A2000. Capability skips are named; supported paths retain original assertions.
