---
id: ctowpe-o944
status: open
deps: []
links: []
created: 2026-11-28T06:07:41Z
type: bug
priority: 2
assignee: kmd
parent: ct-ha8q
tags: [physics, vulkan, regression]
---
# Restore resting contact for the right concave body after the brace collapses

The native Wayland structural visual test reaches the expected delayed failure, but after 600 post-release ticks the right body still moves. The RTX replay measured COM x154.264709 with support witnesses spanning x146.550827 to151.848099, angular velocity0.004516602 and fixed velocities (2023,-3370).

## Acceptance Criteria

Reproduce using the displayed structural_visual_scene_tests fixture. Preserve the complete fall and braced/unbraced load history. At the existing endpoint verify stable support, zero residual motion and unchanged floor contact checks.

