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

**2026-10-14T04:21:24Z**

RED dd5de6a compiled and all3cases failed as intended; GREEN dfca788 GPU staging validates canonical physical/appearance bytes and computes exact four-connected role/object labels with parallel lanes and atomic root hooking. Four GPU checks now pass with Vulkan validation, including staging resize/rollback; spirv-val authored shader passes. This milestone is staging only: overlap/body/geometry/graph/appearance joint reservation and actual publication are still required; ticket remains in progress.

**2026-10-14T04:27:32Z**

Additional proven structural-pixel gaps: structural_capture and damage_members assume rectangle vertex lengths/depth; equilibrium self-weight assumes density*rectangular depth. Exact pixel collider insertion alone is insufficient. Existing graph solve must consume rest endpoints, actual composition mass/COM and a validated section model, before arbitrary structural publication is accepted. Added to audit matrix.

**2026-10-14T05:09:22Z**

External GPU recovery blocker: previouslypassing authored staging timed out45sec exit124 on retry after optimizer unchanged hydro timeout; renderer independently localized successful context/pipeline creation then96-texel first-submit stall. No further GPU launches authorized until recovery. New planning and absent-normal tests compile but were NOT behaviorally RED-verified; preserved as pending tests, no new GREEN implementation. Full feature regressions and publication remain required. See design/physics_feature_checkpoint.md.
