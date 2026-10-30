---
id: ct-xqgz
status: open
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

