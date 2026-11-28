---
id: ctlo-z0zo
status: closed
deps: []
links: [ctlo-jnt7]
created: 2026-10-24T03:41:51Z
type: task
priority: 1
---
# Limit unlit palette updates to visible pixels

## Acceptance Criteria

Albedo-frame GPU work scales with the visible viewport rather than resident atlas capacity. Hidden edits retain fresh optical queries and become correctly textured immediately after panning. Lit mode still refreshes the complete lighting domain. Keep material colors, physical resolution and simulation halo unchanged; verify displayed frame times.


## Notes

**2026-10-24T04:00:15Z**

GREEN focused checks: both albedo-work cases pass, including hidden-edit opacity, correctly textured wood immediately after panning, bounded viewport dispatches, and body slot 65 not aliasing empty coverage. Scene renderer test also passes. Lit mode retains full atlas updates. Actual displayed workload measurement is pending.

**2026-11-28T04:06:51Z**

Verified current albedo_work_tests 2/2 in the completed full suite: one visible dispatch, zero hidden work, fresh hidden opacity, correct wood after panning and exact reference pixels. design/inline_frame_optimization.md records the two displayed 1000-frame visible-albedo runs at 24.079/29.334 and 24.133/29.147 ms mean/worst. These timings apply to the documented level03 workload.
