---
id: ct-ztym
status: closed
deps: []
links: [ct-ww6r]
created: 2026-11-26T05:50:13Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [beaver, gas, save]
---
# Preserve exact gas fields across the live fire demo save and reload

The full Vulkan test run reached real pile ignition and then failed the restored_gas_exact check in tests/app/beaver_fire_runtime_tests.c:150 on the NVIDIA RTX A2000. The saved/restored gas clock and earlier ownership checks passed. The failing run is in /tmp/craft-goal-baseline.log.

## Acceptance Criteria

Reproduce on an isolated save, identify the exact chunk and fields that differ, and verify the existing full per-chunk gas checkpoint comparison passes after reload. Preserve the independent ignition and thermal conservation checks.


## Notes

**2026-11-26T05:50:21Z**

Reproducing run save is build/beaver_fire_test_baxGGV. Before reload the actual pile consumed 4983 fuel units, produced 981 char units, and had 55 flaming parcels. Test reached the per-chunk comparison after the exact clock comparison. The suite was in progress across a rebuild, so confirm the earliest affected revision before attributing the regression.

**2026-11-26T05:51:36Z**

RED confirms the generic camera follow changes the dedicated demo camera on the first physics tick. The failed save has chunk columns -4 through 8 whereas reload at the authored camera uses -1 through 8. Test the fixed authored framing together with the original full checkpoint comparison.

**2026-11-26T05:54:33Z**

RED reproduced first-tick framing drift. The fire demo now opts into its authored camera while ordinary play retains follow. The 330-tick window regression passed with stable camera, real ignition and bit-for-bit gas checkpoints and clock after reopening the save.
