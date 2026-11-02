---
id: ct-vkik
status: open
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

