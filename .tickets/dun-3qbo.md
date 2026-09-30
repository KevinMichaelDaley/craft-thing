---
id: dun-3qbo
status: open
deps: []
links: []
created: 2026-09-29T20:55:56Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, rendering]
---
# Remove remaining midair lateral fan from painted water

The connected falling-stream regression is improved by suppressing false hydrostatic head in fast unsupported water, but the 45-frame capture still shows a broad lateral fan of water around y=45..80 despite the stone floor being at y=110. Diagnose pressure/free-surface transport there rather than hiding it in the renderer.

## Acceptance Criteria

At half-native three-tick cadence, a held high water brush falls as a narrow connected column until it contacts supported water or terrain; before/after GPU captures and a quantitative lateral-width assertion pass, with exact mass conservation and existing river/seam tests passing.


## Notes

**2026-09-30T03:19:38Z**

Added 15/30/45-frame GPU capture diagnostics. At 45 frames before floor contact, widest wet row is 43 cells; 105.9 cell-volumes sit outside x20..44. Fan already forms by frame 15 at the held brush. Marker-off, full unsupported hydro suppression, stale-horizontal-velocity guard, entry speeds 3/8, and lower-volume brush experiments did not meet both connectedness and width checks; all experimental shader changes reverted. Source injection and pressure response need a coherent redesign.

**2026-09-30T03:36:09Z**

User selected a finite-flow held-water source. Keep the initial click responsive, but meter continued GPU injection per physics tick instead of refilling the full brush disk every rendered frame. Preserve the 15/30/45-frame captures, and add a quantitative airborne width assertion once the source remains connected and dense.

**2026-09-30T03:38:03Z**

Exploratory finite-source test: one full radius-6 click then a radius-2 GPU source each tick preserves the falling column gap limit but leaves 25 of 35 near-source rows below 2 cell-volumes. Radius-3 source reduces thin rows to 13, but causes 442 airborne pool cells in the spring spray regression. Both experiments reverted. The finite-flow brush needs its own metered source geometry/velocity and free-surface treatment; simply reusing or enlarging the spring is insufficient.
