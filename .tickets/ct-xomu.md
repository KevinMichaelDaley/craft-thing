---
id: ct-xomu
status: in_progress
deps: []
links: []
created: 2026-10-30T19:20:57Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [performance, save, vulkan]
---
# Batch bonfire shutdown GPU readbacks

After removing the duplicate body checkpoint, shutdown still spends ~8.3s scheduling 187 dirty chunks. Stage trace: chunk download1.7s, color readback2.5s, reactive export2.3s, water0.4s, gas1.2s, queue0.2s. Repeated per-slot GPU readback and queue boundaries dominate. Batch stable state into host staging once where safe, preserve exact snapshots and late-load semantics.

## Acceptance Criteria

One-frame half-native bonfire save flush materially below13s with identical restored chunks, color, reactive, water, and gas; full regression and validation pass; no runtime frame regression.

