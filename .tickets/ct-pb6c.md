---
id: ct-pb6c
status: closed
deps: []
links: []
created: 2026-11-01T02:41:28Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [gas, mpm, physics]
---
# Conserve and relieve gas pressure in MPM-compressed cut cells

The scripted quarter-native smoke-box roof carve develops a 133927 pressure peak after 1000 frames in a cell at world (389,43) with free volume 1/65536, MPM occupancy, and no water. Closing one-quantum voids moves the peak to (388,43), volume 1/4096 and pressure 16389. The benchmark gas audit rejects the resulting acceleration. The gas/MPM interface presently permits dense gas stock to remain in tiny moving-material voids without adequate pressure relief or two-way mechanical work.

## Acceptance Criteria

Add RED GPU and reference tests for moving MPM cut-cell compression, open-neighbor venting, sealed-pocket energy/work, and conservation. Rework gas/MPM aperture or coupling physically; no arbitrary pressure clamp or lost stock. Scripted carved smoke box 1000-frame gas audit passes twice with finite pressure/acceleration and the displayed frame budget. Run make test.


## Notes

**2026-11-01T03:28:31Z**

Claimed after ctlo-08fu checkpoint. Existing gas geometry remap keeps stock in sealed MPM-compressed voids and redistributes only to immediate positive-volume neighbors; gas density/EOS can therefore spike in tiny MPM cut cells. Need RED reference/GPU tests for open vent and sealed pressure-work, then two-way gas pressure/MPM coupling; do not clamp pressure or discard stock.

**2026-11-01T04:02:10Z**

Pressure feedback and sealed-pocket work implemented; carved benchmark still fails with 10k pressure and near-zero transient temperature. Trial 0.64 granular packing cap lowered peak to 2964 but did not pass 10x ambient bound, so reverted. RED pore-volume test retained for proper coupled solution. Do not close.

**2026-11-01T06:50:52Z**

RED GPU tests cover three-cell rigid sweep and wake, MPM pore volume, venting, sealed compression/work, and conservation. Geometry remap now routes gas through connected newly closed cells and fills connected new voids using cached GPU route/demand passes. Granular MPM uses 0.64 random close packing pore model consistently for gas volume and pressure force. Native quarter-resolution smoke-box benchmark passed all closed/open/carved 1000-frame runs twice; carved means 9.020/9.039 ms, worst 19.417/19.769 ms, max pressure 174.673 vs initial 51.907, finite audit. Focused Vulkan geometry tests and make test pass under persistent Xephyr :92; native benchmark used host display per user preference. RED, GREEN, REFACTOR commits 3100e11, d169a46, 4b2067d, f562c35.
