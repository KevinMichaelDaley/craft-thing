---
id: ctowp-v2rf
status: closed
deps: [ctowt-o11u]
links: []
created: 2026-11-27T06:48:09Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Publish a complete native water sweep with one GPU receipt

## Design

Record the pair's private native projection and transport, preflight the entire basin, then publish water mass, carried fields and projected velocities in one GPU submission. Use the same receipt state for both touched owners.

## Acceptance Criteria

Both live owners agree with monolithic water stock, velocity, pigment and heat. A bad cell leaves all live fields and output plan intact. A lost submission acknowledgement fences both owners until the retained receipt is reconciled.


## Notes

**2026-11-27T07:05:09Z**

Both Vulkan configurations publish exact native projected water, pigment, heat, and velocity for the sealed pair. One invalid color rejects the full basin. Loss before submission keeps both owners fenced, lost acknowledgement resolves from either peer, and a surviving owner stays fenced after peer destruction. Four legacy seam cases pass with the shared receipt implementation. All targeted runs used synchronization validation.
