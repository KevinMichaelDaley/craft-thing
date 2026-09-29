---
id: dun-r1a0
status: in_progress
deps: [dun-bd93]
links: []
created: 2026-09-28T17:11:03Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Couple pressure projection across GPU workspace boundaries

Current conservative seam pass uses the pressure and head values from independently projected workspaces. Couple their Poisson boundary condition on GPU so a split liquid domain solves like the same fully resident domain, including hydrostatic rest and velocity continuity.

## Acceptance Criteria

A split-versus-monolithic basin regression has comparable divergence residual and hydrostatic free-surface level; no missing water pixels or spurious seam jets after a long pan; GPU timing stays inside the measured offscreen work budget and there is no per-frame CPU readback.

