---
id: dun-3qbo
status: in_progress
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

