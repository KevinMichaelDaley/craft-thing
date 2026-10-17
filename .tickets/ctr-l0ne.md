---
id: ctr-l0ne
status: closed
deps: []
links: []
created: 2026-10-17T21:50:21Z
type: bug
priority: 2
---
# Preserve leaf artwork on visible leaf flakes after crumbling

## Acceptance Criteria

Leaf bits remain distinct leaf debris, carry their own source RGB through motion and save/load, render as opaque full-size flakes instead of mass-scaled powder; retain light material density.


## Notes

**2026-10-17T22:00:12Z**

RED confirms leaf particles lack source pigment. Leaf fragments now retain source sRGB in material-specific particle flags through transport/persistence, use full-cell opacity at density0.2, and half-cell contact radius. Added exact albedo after relocation and reload; renderer must be explicitly re-enabled after snapshot restore in regression fixture.

**2026-10-17T22:05:49Z**

Latest six carve/leaf tests pass under Vulkan core/synchronization validation, including exact source albedo after moving a fragment to an unrelated cell and after save/load. Pigment bits are excluded from moisture physics; leaf mass stays0.2 and visual coverage is full rest volume.

**2026-10-17T22:51:24Z**

GREEN: leaf flakes retain material10 and their own packed source sRGB through transport/checkpoint; full rest-volume opacity and radius0.5 retain low density0.2. Carve/leaf6/6 pass core/sync, including exact albedo after relocation and reload. Full make test completed; only baseline authored-stage issues remain after separately verified harness/dependency corrections.
