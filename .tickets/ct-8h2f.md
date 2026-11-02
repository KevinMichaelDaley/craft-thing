---
id: ct-8h2f
status: in_progress
deps: []
links: []
created: 2026-11-02T00:31:51Z
type: feature
priority: 1
assignee: kmd
parent: ct-exwr
---
# Route configurable per-player actions into live beaver gameplay and animations

Device bindings feed independent action states, intents, simulation arbitration and animation events in the actual Vulkan app. No animation viewer or test button.

## Acceptance Criteria

Configurable keyboard/controller bindings; stable exclusive device ownership; press hold release and repeat filtering; focus/disconnect/context clearing; viewport aim ownership; movement and authoritative chew/cast phases synchronized with gameplay; frame-six spell delivery once; actual application admission rendering controls and evidence.


## Notes

**2026-11-02T00:34:45Z**

RED 10b106e8 compiles and fails 5/5 cases. GREEN 55ab2142 passes 5 input/action cases plus 24 existing state/asset cases with -Werror. Configurable per-device actions, ownership, edges/repeat handling, context/focus clearing, per-player target, arbitration and latched frame-six ability event implemented. Full make test running in sustained/input-full-tests.log; SDL and actual runtime integration next; ticket remains in_progress.
