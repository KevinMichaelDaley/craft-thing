---
id: ctols-xhuy
status: in_progress
deps: []
links: []
created: 2026-11-27T05:01:33Z
type: task
priority: 1
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, vulkan]
---
# Commit paired liquid flux across shared Vulkan chunk faces

Foreground and offscreen atlases have separate buffers. They need a single commit of the chosen phase flux at a mapped face, including the extensive quantities the current liquid solvers own.

## Design

Validate both mapped endpoints and the full face plan before any stock changes. Use one shared-device dispatch to apply all lanes or reject the batch. Report a distinct unconfirmed result for failed GPU submission so the scheduler can reconcile both owners. The parent supplies paired epochs and the pressure-derived requests and integrates gas and grain ownership.

## Acceptance Criteria

Native tests move water, oil and slime in both directions on real shared contexts, conserve exact stock, heat and momentum, and show a rejected lane or stale page leaves the whole face unchanged. Keep automatic open camera enablement under the parent.
