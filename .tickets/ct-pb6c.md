---
id: ct-pb6c
status: in_progress
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
