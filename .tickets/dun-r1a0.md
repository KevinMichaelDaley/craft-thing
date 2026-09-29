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


## Notes

**2026-09-29T06:32:01Z**

GPU seam now joins one-sided projected face momentum before seam pressure/head flux. Horizontal deep split destination 152.35 vs mono 152.36 cell-volumes after 120 ticks; vertical 984.00 vs 984.00; exact total mass and reverse-flow test pass. UI high-volume test reaches 8 cells into offscreen chunk. Remaining ticket acceptance: directly couple Poisson boundary condition and measure divergence/hydrostatic residual and long-pan seam jets. Commits 7c4aef7, 23b0ff4, 2e9d8df.
