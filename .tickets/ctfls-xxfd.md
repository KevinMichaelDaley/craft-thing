---
id: ctfls-xxfd
status: in_progress
deps: []
links: [ctlo-ehqj, ct-e6fr]
created: 2026-11-26T23:46:13Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-ehqj
tags: [forest, physics, fixtures]
---
# Restore locked forest ignition and archive the displaced kindling setup

The locked forest manifest ignites a radius-four disc at (228,204) with 12000 energy and has no separate pit-floor kindling rectangle. 73af4228 moved the starter to (238,206) and added 21 wood cells at (236,204). The accepted warmth probe stayed at (249,202), so the live fixture no longer isolates the locked inputs.

## Acceptance Criteria

Use the locked starter position, radius, energy and wood footprint in the forest acceptance fixture. Verify the real initial Vulkan inventory, keep the historical setup available as a named comparison, and compare both with the same current gas, clocks, probe and containment checks. Keep warmth and spread acceptance open until the locked runs prove them.


## Notes

**2026-11-26T23:47:40Z**

The manifest wood union contains 90 distinct cells, 37 inside its ignition disc. The portable RED case compiled and exposed the runtime ignition at (238,206) instead of (228,204). The cold Vulkan inventory RED also checks the undeclared kindling cell at (238,206) and the exact authored and starter counts.

**2026-11-26T23:51:52Z**

The actual NVIDIA RED confirmed undeclared material at (238,206) in the otherwise locked still scene. Host GREEN and refactor pass with radius four, 12000 energy and the center at (228,204). The default staging counts unique authored wood cells, and the native audit checks the actual pit cell, all 37 heated body parcels and their total finite energy. The displaced kindling is available only through the named historical control. Native cold audit, matched 180-second runs and spread controls remain due.
