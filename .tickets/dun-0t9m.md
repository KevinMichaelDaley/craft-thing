---
id: dun-0t9m
status: in_progress
deps: []
links: []
created: 2026-09-28T02:30:39Z
type: feature
priority: 1
assignee: kmd
parent: dun-oa1i
---
# Keep nearby offscreen chunks evolving with distance-based GPU cadence

Viewport plus one-chunk halo currently defines the entire simulated GPU grid. Chunks save and sleep immediately beyond it, causing a visible physics seam when the camera pans. Introduce a bounded nearby offscreen simulation cache/scheduler: visible physics at normal cadence, nearby one- and two-screen bands at lower cadences, and dormant chunks only many screen widths away. Preserve GPU-only physics and threaded chunk streaming; no normal-frame CPU readback.

## Acceptance Criteria

A water or grain state moved one or two screen widths offscreen advances before the camera returns; stepping cadence decreases with distance and eventually becomes dormant; transitions conserve mass/particles/velocity and do not create a viewport seam; no full expanded-grid allocation or per-frame CPU readback; end-to-end camera tests and benchmark pass.

