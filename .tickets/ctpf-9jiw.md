---
id: ctpf-9jiw
status: open
deps: [ctpf-iueo]
links: []
created: 2026-10-14T03:55:22Z
type: task
priority: 1
parent: ctpf-rvcy
tags: [physics, structural, fracture, authoring]
---
# Implement missing authored-structure release and fracture ownership behavior

Use audit-defined gaps, existing material failure mechanisms and optimizer-owned parallel kernels. Preserve arbitrary authored object/source texel and emissive provenance through delayed structural release, damage/joint failure, exact fracture and weak-material crumble.

## Acceptance Criteria

Analytical force/moment support, grace/equal-time behavior, correct sound detach/split/crack/crumble and conservation including momentum/fracture work. Rotated mixed concave inputs and saturated retries preserve one owner and survivor materials/appearance. Existing cuts not duplicated.


## Notes

**2026-10-14T04:36:59Z**

Proven new angled-authored gap: current pixel fracture axis0/1 assumes beam orientation baked into body pose; imported exact grid colliders can have diagonal graph/grain direction at angle0. Need graph-derived body-rest normal/offset cut through optimizer-owned common parallel fracture job, with exact surviving material/provenance and bond work/conservation. Do not copy or rewrite serial BFS.

**2026-10-17T21:08:44Z**

Runtime component conversion/carving/fracture now use edge-and-corner connectivity (ctr-xhp4). Remaining authored admission/component planning must use the same connectivity within authored object identity, preserving complete component publication; design/physics_feature_split.md updated.
