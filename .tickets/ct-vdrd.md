---
id: ct-vdrd
status: open
deps: []
links: []
created: 2026-10-30T18:32:14Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [vulkan, performance, rigid]
---
# Reduce repeated rigid solver work in sparse bonfire scenes

RTX A2000 half-native bonfire frame trace shows rigid GPU about 11ms, with position solve/apply dominating. Quarter detailed trace measured phase6 solve ~1.8ms and phase7 apply ~1.3ms across 8 substeps and 8 iterations. Existing fused-grain option worsened half-native mean to35.130ms. Profile exact active contact/body work and cut redundant dispatch/barrier cost without changing physical clock, substep count, or contact solver results.

## Acceptance Criteria

Focused solver/catapult/bonfire physics tests pass with zero validation errors; displayed half-native 32-frame normal-clock timing improves mean and worst on this GPU; no fixed body counts or numeric-policy shortcuts.

