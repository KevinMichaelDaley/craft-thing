---
id: ct-xuqp
status: open
deps: []
links: []
created: 2026-10-28T23:31:04Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-6syy
tags: [renderer, physics]
---
# Render full admitted wooden catapult arm with wood color

The cold catapult display test proves the admitted 16x2 wood pixel body occupies x64..79 and rotates after release, but ALBEDO screenshots show only an approximately eight-cell dark strip. Authored pixel color edits before and after admission and a body-ID override in scene.comp did not color the strip, indicating a coverage or body lookup defect. See build/screenshots/catapult_cold_cocked.bmp and catapult_cold_flight.bmp.

## Acceptance Criteria

ALBEDO rendering covers the full moving arm with a visible wood material/color at cocked and released angles; rendered arm geometry matches occupancy and remains visually separate from the stationary grounded base and target wall. Add a display regression that checks captured pixels against rigid pose.

