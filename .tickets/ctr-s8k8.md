---
id: ctr-s8k8
status: closed
deps: []
links: []
created: 2026-10-17T21:50:21Z
type: bug
priority: 2
---
# Release connected trees when authored soil or gravel support is removed

## Acceptance Criteria

Initial authored terrain stays fixed; removal of original granular support releases the entire wood/leaf component; repair old saved support history without restamping edits; Vulkan regression.


## Notes

**2026-10-17T22:00:12Z**

RED reproduces fixed wood/leaf component after removing soil/gravel underneath. Support bonds now record all solid neighbors while component union still groups rigid-capable solids only. Old-save migration recovers original granular support history from asset without restamping; sparse MPM support changes invalidate graph only next to held terrain. Added direct erase and natural support-motion coverage.

**2026-10-17T22:05:49Z**

Latest root regression passes all four cases: soil/gravel removed with air brush and soil/gravel moving away under MPM. Old-save missing-support migration test passes. Core/sync validation follow-up and actual saved level03 replay now running.

**2026-10-17T22:25:08Z**

Saved level03 replay exposed a second cause: rigid contacts used full owner-cell boxes for grains, and loose particles were treated as infinite-mass terrain. RED regression reproduces a trunk held by unanchored grains (y=10.0006 after 12 ticks). Implementing actual per-layer particle footprints and parallel finite-mass two-way coupling; saved-world replay is the completion gate.

**2026-10-17T22:29:01Z**

Actual saved level03 full-tick replay now falls: trunk y 60.4514 -> 68.1114 over 8 ticks, rather than hovering at 60.3. No core/synchronization validation errors or solver overflow. Per-particle finite-mass corrections use parallel integer reductions in reused MPM proposal storage. Combined MPM/rigid displacement bounded to the gather halo; mass-conservation regression rerunning.

**2026-10-17T22:51:31Z**

GREEN and refactor verified: final saved level03 full-tick replay with core/synchronization validation moves trunk y60.4514 ->68.1114 over8 ticks, no errors/overflow. Trapped-grain regression trunk y10 ->25.763 and exact20x78643 mass conserved. Sequential detachment5/5; contacts10/10; rigid solver13/13; grain dispatch ownership10/10 validated. Grain coupling is parallel, finite-mass, two-way, uses actual centers/radii/layers and reused scratch; inactive work skipped onGPU. Original soil/gravel support history migrates without restamping. Full make test completed; two introduced dependency/dispatch-fixture failures corrected and verified separately; remaining baseline REDs tracked ctr-ojvy.

**2026-10-17T23:02:56Z**

Reopened after tree penetrated stone. Thousands of leaf/grain constraints diluted rigid contact relaxation equally despite their tiny body response. RED dense-leaf body penetrates floor to48.5969 (surface48). Separate conservative particle relaxation and mobility-weighted rigid degree now stops it at47.9842; core/sync validation clean. Adding full level03 settling/cut/floor integration beyond earlier eight-tick replay.

**2026-10-17T23:21:03Z**

Final GREEN: component aggregation distinguishes partial natural granular support motion from complete support loss; existing release bit records explicit cutting. Rigid rows use binding-set degree and grain mobility fraction, while particle/speculative rows keep conservative relaxation. Dense-leaf floor stops at47.9842 for floor48, rather than48.5969. Actual level03 retains initial tree after90 ticks, then trunk cut and240 further ticks show a falling tree stopped at rock, final zero wood-body pixel centers in stone (max2 transient sampled centers), validated under core/sync. Full suite only baseline ctr-ojvy two REDs. New regressions and actual rendered screenshots are reproducible from repo tests.
