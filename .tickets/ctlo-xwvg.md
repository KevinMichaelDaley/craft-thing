---
id: ctlo-xwvg
status: in_progress
deps: []
links: []
created: 2026-10-19T21:46:40Z
type: task
priority: 1
parent: ctr-ebl5
tags: [vulkan, performance, rigid]
---
# Gate rigid grain-bed work from GPU body and particle activity

Strict full-frame tail remainsRED: first640frame43-51ms, with rigid solver12ms versus2.5ms warm; grain-bed init/pointer doubling runs over the full grid even with no particles or active bodies. Add a one-group GPU planner and owner-specific indirect work commands; no CPU pixel walk/readback or fidelity reduction.

## Acceptance Criteria

GPU counters gate grain-bed init/jumps to zero for an empty particle pool or no active bodies, and immediately restore full work when bodies and grains are present. Empty rigid per-body phases may use GPU body-work triplet. Preserve pressure-bearing powder interactions, exact physics tests, dynamic bound inputs, core/sync validation, graphical capture and strict<30ms full-frame gate.


## Notes

**2026-11-28T07:18:12Z**

Native GPU grain planner 3/3, exact dense support 7/7 and real Wayland visual 1/1 passed core/synchronization validation. The compact graph matched all 16384 float words and the graphical test preserved 7320 optical samples under restyling, exact masses and four paused frames. Three consecutive 24-frame simulation/render benchmark pairs passed the 30 ms gate. A matched actual swapchain variant with queue completion every frame passed both resolutions twice, then at 480x270 exceeded the cap (45.578 ms). The 640x360 presented maxima were 19.492, 19.157 and 28.709 ms. The strict combined-frame tail remains intermittent.
