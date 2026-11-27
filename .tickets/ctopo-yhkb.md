---
id: ctopo-yhkb
status: in_progress
deps: [ctols-vhg9, ctopp-fkir, ctopp-2c14, ctowpe-3271]
links: []
created: 2026-11-27T05:39:32Z
type: task
priority: 1
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, physics]
---
# Plan liquid seam flux from matching projected faces

The level can commit a signed face plan at a matching owner interval. A normal camera tick still needs to derive that plan from the current liquid solver and coordinate when each owner reaches the face.

## Design

Use current face velocities, pressure and aperture from both endpoints, together with current phase density and rheology. Reuse the interior pair rules for stock and momentum. Complete and apply the paired face before either owner advances the next interval.

## Acceptance Criteria

Compare split and monolithic basins across horizontal and vertical faces with forward and reverse flow. Verify actual volumes and pressure work under matched intervals, including blocked and shallow apertures, and preserve the paired rejection and recovery tests.


## Notes

**2026-11-27T05:39:32Z**

The durable owner binder expects both endpoints at the same completed interval and closes paired edits through the existing world transaction. The parent retains gas species and pressure energy, grain coupling and repeated camera admission.

**2026-11-27T06:02:05Z**

The pressure checkpoint uses the native projection on a sealed two-tile domain. ctopp-2c14 covers extracting and applying water transport. This parent retains the coupled oil and slime planner, larger resident pressure neighborhoods, and scheduling both owners at the same transport interval.
