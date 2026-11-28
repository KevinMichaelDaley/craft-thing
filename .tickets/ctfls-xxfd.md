---
id: ctfls-xxfd
status: closed
deps: []
links: [ctlo-ehqj, ct-e6fr, ctgwpe-1n73]
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

**2026-11-27T00:07:02Z**

Exact 19ca2597 passed the NVIDIA cold acceptance fixture. The undeclared pit cell was air, 37 authored parcels were lit and their measured starter heat was 11999.999481. The live ember was 980.3 K. Long still, historical and spread comparisons remain open.

**2026-11-28T06:55:26Z**

Completed both 180-second still-air comparisons using the same frozen RTX executable (SHA256 25e3e4d470b31075f67831b91f2298e2825a8c3f77b17360616f94a51a29315c), physics, probe, clocks and containment checks. The live cold inventory measured 37 lit parcels and 11999.999481 energy. Both runs yielded all 19 scheduled samples and zero outside flame in every sample. The locked configuration missed the manifest warmth band in 3 of 11 samples (292.49 to 327.74 K); the named archived control missed in 6 of 11 (293.51 to 349.18 K). Mass deltas were -0.0007 and -0.0004 respectively. The locked east and west containment controls also completed 180 seconds. Fixture restoration and comparison are complete. Warmth remains open under ctlo-ehqj and spread remains in progress under ct-e6fr, with its dedicated control run started.
