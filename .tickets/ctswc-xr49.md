---
id: ctswc-xr49
status: open
deps: [ctsct-a986]
links: []
created: 2026-11-27T01:29:57Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [offscreen, vulkan, world-clock]
---
# Port time-bounded offscreen workspaces to the current Vulkan device

The confirmed world epoch and chunk timestamps can now carry an offscreen handoff. Port workspace creation and bounded elapsed stepping from archive 7347531 using the current Vulkan device and body storage.

## Design

Reuse the existing chunk coordinate and generation checks during ownership handoff. Advance only known stale snapshots in bounded solver steps, update completed time only after successful GPU work, and keep newer state safe on retry. Leave foreground promotion gated until channel and seam conservation are verified.

## Acceptance Criteria

Vulkan tests cover workspace creation and teardown, exact elapsed stepping without duplicate retries, idle and unknown chunk handling, and unchanged physical stock for an isolated tile across a cold save and return. Track cross-workspace conservation and production enablement in the parent.

