---
id: ctowpe-qtwp
status: open
deps: []
links: []
created: 2026-11-28T06:46:29Z
type: bug
priority: 2
assignee: kmd
---
# Diagnose saturated camera admission stalling in the complete snapshot suite

The native RTX test_level_structure_snapshots run stopped the irregular-frame fixture at its 120-second no-progress gate before admitting any owners or visible slots. A focused run of the same executable with the same Wayland compositor, cache and Vulkan synchronization validation then passed. The failure precedes fractional replay and stock comparisons.

## Acceptance Criteria

Identify the admission stage that stops making progress in the complete suite, preserve its deadline and world ownership semantics, and verify both the complete sequence and the focused irregular-frame case.

