---
id: ct-7kca
status: open
deps: []
links: []
created: 2026-10-30T10:34:54Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
---
# Keep thermal working grids in device-local Vulkan memory

At half-native, the MPM and thermal GPU stage takes about 655ms per displayed frame. Thermal samples and fluxes are each 32 bytes per active cell and allocated host-visible, which chooses system RAM on this NVIDIA device once its 246MiB mapped-VRAM aperture is exhausted. Move the hot working grids to device-local memory and stage only explicit inspection readbacks; benchmark the real displayed bonfire and preserve thermal results.

## Acceptance Criteria

Thermal samples and fluxes allocate DEVICE_LOCAL; explicit inspect still returns exact state through a readback; thermal feedback and checkpoint tests pass; GPU timing on X11 half-native is measured before and after with normal physical clock.


## Notes

**2026-10-30T10:38:58Z**

Experiment measured on RTX A2000 X11: moving thermal samples and fluxes to DEVICE_LOCAL and staging inspection readback passed thermal 3/3 with validation, but the eight-frame half-native displayed mean was 1122.480ms (baseline 1133.923ms) and MPM/thermal stage remained 650-682ms vs baseline about 655ms. This does not materially fix the bottleneck; implementation and test were reverted. Investigate pass count/algorithm and rigid tick rather than memory placement.
