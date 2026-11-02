---
id: ct-ww6r
status: open
deps: []
links: []
created: 2026-11-02T02:16:50Z
type: bug
priority: 1
assignee: kmd
parent: ct-9ieb
---
# Enable fire-demo gas before restoring saved chunk checkpoints

Isolated replay of preserved user demo save fails Player setup: Invalid cold gas checkpoint import. level service imports gas packets before dc_beaver_fire_setup enables gas. Need initialize matching model/policy before chunk admission and regression restore seeded burning world without reinitializing stock. Separate from live numerical failure ct-1dxt.

