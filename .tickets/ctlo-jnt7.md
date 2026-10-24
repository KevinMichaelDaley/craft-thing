---
id: ctlo-jnt7
status: in_progress
deps: []
links: [ctlo-pwxh, ctlo-z0zo, ctlo-32z4]
created: 2026-10-24T02:19:33Z
type: bug
priority: 1
---
# Parallelize air-brush fragmentation of large colored bodies

## Acceptance Criteria

Large colored pixel bodies carve and split with exact pixels, material, appearance and motion retained; cooperative GPU connectivity, geometry copies and mass properties replace single-lane whole-body work; displayed brush workloads remain within the 30 ms frame target without disabling controls or lowering resolution.


## Notes

**2026-10-24T02:23:16Z**

RED: the checked-in large-body fixture on Iris Xe removes exactly 49 pixels from 36,100 wood pixels and retains 21,630.6 mass, but the brush transaction takes 661.080 ms against a 30 ms limit. Implementing cooperative connectivity, alternate-bank copies and mass properties. Earlier normal displayed replay averaged 25.256 ms but had rare 49 ms tails; live input independently reproduced 110–125 ms paint stalls. The target remains strict.

**2026-10-24T02:41:46Z**

The cooperative 128-lane implementation passes 8/8 carve correctness tests with Vulkan core and synchronization validation. The large 36,100-pixel carve takes 17.743 ms (previously 661.080 ms). The actual displayed tree-cut workload still fails over 500 frames: mean 45.942 ms, worst 82.504 ms; steady painting costs about 8 ms even on misses, with simulation around 35 ms. A new RED test measures a missed brush with 320 bodies at 29.996 ms against a 3 ms budget, demonstrating per-slot barrier scaling. Parallel candidate filtering and a dedicated carve shader are next. The strict 30 ms full-frame gate is unchanged.

**2026-10-24T03:26:13Z**

Additional RED tests: a warm missed brush still submits twice (1.980 ms), and scratch growth after a cached solver tick leaves descriptor_revision unchanged. The lifetime test fails before reusing the stale commands. It also checks that the retained body remains visible in a paused albedo render after growth. Clipping now passes 55,296 exact cases with five reads per query; full displayed frame acceptance remains pending.

**2026-10-24T04:00:15Z**

GREEN focused validation: 12/12 body-carving tests pass with Vulkan core and synchronization validation. Large 36,100-pixel carve: 14.894 ms with 36,051 pixels and exact expected mass retained. Warm miss with 320 bodies: 1.719 ms and one submission. Tests cover cached-command invalidation and paused body lookup after work growth, camera reprojection while continuing gravity, colors, allocation rejection/retry, compound cavities, canopy preservation, leaf particles, and GPU water-source isolation. Five clean-build/header dependency checks also pass. Overall displayed 30 ms worst-frame acceptance is still outstanding; do not close this ticket.
