---
id: ctcdo-kld7
status: in_progress
deps: [ctdot-kbys]
links: []
created: 2026-11-27T04:17:01Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, structural]
---
# Transfer dynamic bodies and structural bonds between camera and offscreen owners

Camera integration currently gates automatic workspaces to stationary isolated chunks. A body or support graph spanning the boundary remains resident-owned until an explicit handoff exists.

## Design

Transfer body and member identities, contact state, transforms, momentum and external bond edges at a confirmed epoch. Acknowledge the owning generation before either solver resumes and keep cold recovery atomic for the entire transferred group.

## Acceptance Criteria

A moving body and a spanning bonded assembly cross the camera boundary in both directions with one authoritative owner throughout. Verify time, momentum, bonds, failure retry and cold reopen on the RTX native path.
