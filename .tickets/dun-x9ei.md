---
id: dun-x9ei
status: open
deps: [dun-0p7p, dun-cwhn, dun-9qub]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid]
---
# Solve rigid contacts against terrain and bodies

Generate contacts from nearby material/occupancy cells, resolve impulses/penetration in bounded iterations, and document speed/substep limits.

## Acceptance Criteria

Body rests on terrain, reacts to removed support next tick, and does not penetrate under supported bounds; body-body overlap is resolved.


## Notes

**2026-10-08T06:54:15Z**

Follow-up contact regression to cover: a body whose reference point is in a missing page can still overlap a resident page and another body. The new XPBD solver freezes integration/application for that asleep slot, but contact effective mass/inertia and velocity currently use its stored Body values. Test streamed-page transitions and treat a frozen slot as zero inverse mass/inertia with zero contact velocity while asleep; retain its stored state for reactivation. Also cover support removal and initially overlapping body separation, which are this ticket's remaining acceptance cases.
