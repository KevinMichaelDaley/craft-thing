---
id: ctpf-iueo
status: in_progress
deps: [ctpf-753m]
links: []
created: 2026-10-14T03:55:22Z
type: task
priority: 1
parent: ctpf-rvcy
tags: [physics, structural, fracture, authoring]
---
# Implement atomic arbitrary-pixel terrain rigid and structural authoring

Accept validated material/role/anchor/object maps and explicit state variant. Exact four-connected rigid bodies, ordinary terrain, explicit marked structural graph/member/joint mapping with real roots; joint physical/visual preflight and reservations.

## Acceptance Criteria

Mixed concavities/holes/disconnected components, material-derived mass/inertia/buoyancy, truthful structural support and rotated/sideways loads. Overlap/capacity/UNKNOWN/idempotence failure fully rolls back; no CPU runtime component solve or new serial scans.


## Notes

**2026-10-14T04:12:17Z**

Authoring admission ABI agreed with renderer and optimizer. Three GPU staging acceptance cases compile; RED execution queued after renderer regression and optimizer native checks. Implementing canonical channel validation and four-connected role/object labels on a separate GPU staging path before shared all-role reservation/publication. No gameplay placement completion yet.
