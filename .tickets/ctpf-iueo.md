---
id: ctpf-iueo
status: in_progress
deps: [ctpf-753m]
links: [ctfs-0zw5]
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

**2026-10-17T09:03:35Z**

Renderer checkpoint integration857ec8b now includes actual prop reader/exporter. Exclusive recovered GPU execution with core+sync validation observed3pass2fail in authored_stage_tests (no validation diagnostics): component-plan getter remains a stub and absent-normal default semantics fail. These pending tests are now behaviorally RED, replacing prior unlocalized stall evidence. No claim of publication/reservations or completed feature. Combined all-test run continues past those known failures to check remaining consumers; unchanged tests retained.

**2026-11-28T05:22:14Z**

Two new GPU failures reproduced: a descriptor-pool allocation failure during resize destroyed the previous ready component plan; preflighting a terrain-water asset woke the live fluid scheduler before publication. The replacement stage now completes before installation, releases partial allocations on failure, retains the prior component plan, and removes the per-pixel CPU water scan from staging. All eight authored-stage cases pass on RTX A2000 with Vulkan core/synchronization validation, including exact memory accounting and successful retry. Complete atomic structural publication remains open.

**2026-11-28T05:37:17Z**

Successful terrain-prop admission now publishes the presence of committed water from the GPU. A linked regression counted CPU pixel reads on placement before the fix; all eight final terrain-prop cases pass with zero such reads, dry-state preservation on rejection, 1,504 water pixels and exact material/pigment checks. Input file validation is still performed on admission.

**2026-11-28T06:09:57Z**

GPU preflight now checks that each declared member owns a valid structural pixel and that each member, joint and anchor endpoint resolves uniquely to a stable node. Real red cases admitted an unowned member and an unresolved endpoint. Duplicate node identity and zero generation are rejected. The final ten staging tests, including corrected-input retry, pass on RTX A2000 under core and synchronization validation. The compiled shader passes spirv-val. Live all-role reservation, root contact and publication still require implementation.

**2026-11-28T06:25:42Z**

Reproduced structural staging accepting canonical dense wood with an ordinary wood mechanical profile, in disagreement with fr_structural_material_map. GPU staging now rejects materials without a registered structural profile and preserves all rigid-role material support. All 11 staging cases passed on the RTX A2000 with synchronization validation, as did an expanded 36-combination check covering twelve canonical material IDs against each of the three declared profiles. SPIR-V validated. Atomic live placement remains open.

**2026-11-28T06:42:28Z**

The GPU preflight admitted a NaN joint stiffness. It now validates all eight mechanical coefficients and the orientation of every joint and anchor in parallel. Six native failure cases cover nonfinite stiffness, strength and orientation and negative capacity/friction. Valid signed angles and corrected retry pass. All 12 authored staging cases passed RTX core/synchronization validation; SPIR-V validates.
