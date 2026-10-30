---
id: ct-lmzc
status: open
deps: []
links: []
created: 2026-10-30T10:42:38Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
---
# Bound bonfire thermal passes to active tiles

RTX A2000 half-native GPU timestamp trace shows ~82ms sample, 77ms preparation, 172ms transfer, 127ms bonfire, 93ms ignition and 120ms reaction groups per tick. These costs span repeated full-grid passes over 1088x704 halo despite only two small bonfires. Introduce conservative GPU active-tile scheduling while preserving exact heat, wetness, reaction, rigid ownership, and wake-up on painting/streaming.

## Acceptance Criteria

RED tests cover dormant air tiles, newly painted heat/wet tiles, rigid/constructed wood moving between tiles, and exact reaction stock across chunk seams. GPU dispatch count and displayed X11 half-native frame timing improve materially without lowering clock or render/physics resolution; full relevant tests pass.


## Notes

**2026-10-30T16:12:02Z**

GPU memory placement reduced traced half-native thermal from ~650ms to ~13ms per tick. Active tile work may still provide sustained frame headroom, but is no longer first bottleneck; gas ~11-12ms and rigid work now dominate near-30ms frame. Do not implement tile scheduler based on pre-placement timings.
