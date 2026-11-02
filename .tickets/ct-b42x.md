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

**2026-11-02T02:00:51Z**

Latest user explicitly prefers zero catch-up. Current helper admits at most ONE fixed world tick/display, retains only sub-tick pacing fraction and discards whole missed-tick debt. Repeated250ms stalls and one press produce one event/no backlog;3 clock cases pass. Actual normal --play before15ticks/frame284-305ms; after1tick/frame22-24ms. Inactive rays submit no GPU work (regression3/3), avoiding unconditional gas matrix/cache invalidation. Direct demo now uses exact existing tools/smoke_profile process-local defaults with overrides preserved (3 parity checks pass). Same unlit scene before103.823ms, production profile64.929, current59.429; actual-window workload idle58.498/move60.469/cast97.357/burning71.266ms. GPU before gas~64ms, reactive/MPM17-18ms, render6-8ms; no repeated compile/allocation in measured steady frames. Residual massive-demo GPU cost remains unresolved; do not claim30ms budget met. Physical simulation slows relative to wall time under overload; action animation uses actual presentation time without world replay.

**2026-11-02T02:09:09Z**

CPU-only follow-up: active fire-ray heating invalidated gas transport and pressure caches every active tick. Existing thermal pass preserves these for PRACTICAL policy because heat changes no mass/momentum/geometry. New mocked production-host path plus numerical thermodynamics oracle compiled RED3pass1fail then GREEN4pass0fail; practical keeps caches and acoustic policies still invalidate. Commits26874b2f/ebdf7952. No live binary/SPIRV rebuilt, no GPU activity. Actual measured frame-time effect and fullsuite pending user demo closure; residual scene cost remains open.
