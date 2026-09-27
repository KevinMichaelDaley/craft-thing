---
id: dun-zx4t
status: in_progress
deps: []
links: []
created: 2026-09-27T01:41:30Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, fluid, mpm, ui]
---
# Damp residual water and grain velocity; add interactive dirt floor

Reduce persistent bouncing with tiny velocity damping and expose a soil floor in the coupled interactive scene.

## Design

Apply the same small post-solve velocity loss in GPU water and granular passes without CPU readback; seed a supported dirt surface in the coupled demo.

## Acceptance Criteria

Water and sand horizontal motion decay under isolated conditions; the coupled demo visibly includes a dirt floor and permits wetting it into mud; screenshots and interactive launch run through the Vulkan path.

