---
id: ct-j34i
status: closed
deps: []
links: []
created: 2026-11-26T05:42:31Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver, world]
---
# Route camera follow and reticles through the active player viewport

The 3811d47f follow block pans the world around player one while split view is active and converts absolute 64-bit chunk positions to floats. Controller targeting uses the full view center instead of each player viewport.

## Acceptance Criteria

Exercise movement, chunk transitions, editor camera controls, split-screen framing and per-player targets near far and negative coordinates in the actual application.


## Notes

**2026-11-26T05:47:40Z**

First stage separates editor/UI camera ownership from single-player gameplay follow and computes view-relative positions before converting from signed chunk coordinates. Split-screen viewport sizing, tracking and per-controller aim remain in scope.

**2026-11-26T05:51:42Z**

The live camera test reproduced editor drift and passes once gameplay alone drives camera follow. Published view positions now subtract signed world chunk origins before floating-point conversion. Split layout and reticle ownership remain under this ticket.

**2026-11-26T09:14:13Z**

Start joins local controllers and activates their viewports. The shared streaming window follows the group, and each aspect-correct crop follows its own body horizontally and vertically. Mouse and stick aim use the renderer crop before converting to canonical world cells. The crosshair is composited only in its owning window band. Editor mode uses the manual full view, and disconnected controllers lose their aim. Live tests cover two moving players crossing chunks, a jump, four-way clamping, exact aim more than one million chunks west, and rendered crosshair isolation. Final split 8/8, player runtime 3/3, player actions 5/5, live chew 1/1, renderer split 5/5, presentation focus 3/3 and default recipe 2/2 passed. GPU dependency checks passed 9/9. Separate far-origin grab/carry/throw arithmetic is tracked in ct-m0uq. The full test run is still active under ct-il37.
