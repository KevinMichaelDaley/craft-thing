---
id: dun-r1a0
status: closed
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

**2026-09-29T09:45:06Z**

Coupled the GPU Poisson correction on an eight-cell strip across each shared workspace boundary (32 red-black SOR sweeps in workgroup memory, correcting pressure and both velocity components before conservative transfer). Split 120-tick basin destination 145.34 vs monolithic 145.38 cell-volumes; summed seam divergence 0.805 vs monolithic 0.418 (baseline split 3.702). Transient seam level step narrows from 1.524 cells at 120 ticks to 0.178 at 240 ticks. Damped 32-cell river surface 32.189/32.155 split vs 32.380/32.270 mono, exact mass, zero deep dry holes/high spray. Five-second camera pan keeps 30.34 cell-volumes at least eight cells into offscreen destination. Boundary dispatch 0.191 ms; half-native 61.53 physics ticks/s. make test test_ui test_ui_long test_half_native all pass; no per-frame CPU readback.
