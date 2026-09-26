---
id: dun-1v7q
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, foundation]
---
# GPU runtime and simulation pass graph

Build the C11 Vulkan/SPIR-V runtime, headless path, fixed-tick scheduler, and explicit rigid -> Eulerian fluid -> falling-sand pass graph. See ENGINE_PLAN.md.

## Acceptance Criteria

Validation-clean headless and windowed runs; stage order is explicit and captured; per-stage GPU timings and state readback are available; shaders are built to SPIR-V.

