---
id: ct-vkik
status: in_progress
deps: [ct-xsll, ct-ortp, ct-hk3v, ct-unkh, ct-1i55]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Validate playable beaver animations, reactive pixels and local co-op end to end

Run acceptance against the real application and document reproducible controls, target selection, screenshots and material behavior evidence. Cover forest/tree harvesting, test spells, movement, split-screen and independent player state.

## Acceptance Criteria

All 6/8/8 animations visible; A/D/Space/Q/E and documented chew input work. Two local players can move/cast/harvest independently in split-screen. Robe and hat singe; fur wets and ignites and keeps state across animations. Fire bolt and ice storm alter real materials. Wood inventory conservation verified. Full make test attempted, failures triaged into concrete tickets, no claims beyond actual evidence. Changes committed and pushed.


## Notes

**2026-11-26T21:06:48Z**

Integrated 3c94b591 passed seven live runtime, ten split-screen, six live action, three authored spawn and one live fire-ray cases. Both extra native acceptance probes passed: the keyboard and controller harvested different cells and received exactly 39,322 Q16 wood each; simultaneous casts produced one real burning coal body and 65,536 Q16 ice-storm fluid at the controller target. The authored runtime tests generated actual split-start.bmp and split-walk.bmp in the integration worktree. Full make test for 3c94b591 is queued behind the earlier published gas and pose runs. The current hotbar uses 2 then Q for keyboard ice and E for dropping a carried body.
