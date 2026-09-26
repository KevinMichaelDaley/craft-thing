---
id: dun-kjp9
status: closed
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-1v7q
tags: [gpu, build]
---
# Initialize Vulkan device, SPIR-V build, and headless compute path

Create a C11 Makefile build, device/feature checks, shader compilation, validation-enabled development mode, and one headless compute dispatch.

## Acceptance Criteria

Shader writes a known cell pattern; GPU readback matches expected values; missing Vulkan requirements produce specific errors.

## Notes

**2026-09-26T05:37:44Z**

End-to-end compute readback and interactive Vulkan swapchain smoke pass; validation layer reports no errors. Make is used throughout.
