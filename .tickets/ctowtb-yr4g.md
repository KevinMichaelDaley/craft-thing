---
id: ctowtb-yr4g
status: closed
deps: [ctowp-v2rf]
links: []
created: 2026-11-27T07:11:47Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Finish native water correction inside the paired publication

## Design

Gather the two active marker banks and cell seeds into the paired solver. Run the native mass correction, marker advection and velocity damping after advection, then publish the corrected fields and markers under the existing batch receipt. Keep advection pressure work separate from marker-driven exchanges.

## Acceptance Criteria

Compare marker-enabled split basins with the complete monolithic water cycle, including different active marker banks, both axes, conserved carried state, unchanged owners after rejection and publish/receipt recovery.

## Notes

**2026-11-27T07:36:58Z**

Both sealed axes match the native marker cycle, including active marker banks, correction guides, damped face velocities, water stock, pigment and heat. The horizontal fixture forces a 8192-unit Q16 correction across the face without counting it as pressure work. Vulkan synchronization validation passed 18 targeted cases over default, marker-correction-disabled, cached generic, projection, color and receipt paths, including two displayed staged-save cases. 81 host tests, 54 GPU tooling tests, six SPIR-V validations and both ASan/UBSan cases also passed. The full suite is queued on the same source revision for available GPU memory.
