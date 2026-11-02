---
id: ct-b42x
status: in_progress
deps: []
links: []
created: 2026-11-02T01:23:53Z
type: bug
priority: 0
assignee: kmd
parent: ct-exwr
---
# Profile and repair severe live beaver demo frame regression

Urgent user report sudden awful performance. Measure named scene/process, idle/move/cast plus unlit/ignited wood pile CPU/GPU/frame and queue work. Preserve actual materials, fire, animation, framing and controls; compare same workload before/after and record cause. Avoid concurrent test/game workloads, no unrelated process termination.


## Notes

**2026-11-02T01:29:08Z**

Confirmed severe catch-up regression in unmodified normal --play loop: isolated wrapper records 15 world ticks per displayed frame and 284-305 ms/frame after overload. Same binary bounded one-tick benchmark: plain --play mean21.419ms/worst22.372; new unlit fire-demo mean103.823ms/worst110.065. No other game/test process at measurement; unrelated user applications left untouched. Investigating shader/frame/gas pass costs plus redundant inactive ray invalidation. Need bound catch-up scheduling and retain fixed tick numerical fidelity; all graphics/materials stay enabled.
