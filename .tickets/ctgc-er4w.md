---
id: ctgc-er4w
status: closed
deps: []
links: []
created: 2026-11-26T16:25:36Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, display, vulkan]
---
# Measure the first-second visible footprint of the Greek siege shot

The existing coal diagnostic samples a single interval endpoint and compares raw aerosol and reaction power. The Greek siege parent has no check for the peak of the actually composited smoke and flame union.

## Design

Measure displayed smoke alpha and visible linear flame power on each presented tick of the physical prompt shot. Read the locked thresholds and viewport cap. Keep the fixture clock and actual pouch release in the evidence.

## Acceptance Criteria

A real displayed prompt shot has a positive visible fire signal and covers at most a quarter of the viewport on every frame of the first simulated second. Include threshold, overlap, viewport and transient burst regressions.


## Notes

**2026-11-26T16:38:51Z**

The native RED run opened the pouch at tick 26 and completed all 60 frames after release, but submitted no visible flame because the shared siege setup left the thermal diagnostic selected. The normal compositor now renders in siege scenes. The locked native target passed with peak union 7447 of 129600 pixels (5.75 percent), well inside the locked 25 percent. At the peak, 7438 smoke and 614 flame pixels overlapped at 605 pixels. The Vulkan regression checked the nonzero viewport origin, presentation readiness, alternate thresholds, clipping, overlap, the diagnostic overlay, and clearing on the next frame. Fixture regressions reject a transient burst even if terminal coverage is low.
