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


## Notes

**2026-09-28T02:34:03Z**

User clarified cadence policy: offscreen simulation remains on the same world clock, using larger timesteps and fewer solver iterations at greater distance. Respect velocity/CFL stability; use extra substeps when necessary rather than silently slowing world time.

**2026-09-28T02:42:56Z**

RED end-to-end pan test proves water one screen offscreen remains frozen for 60 ticks. Design in design/offscreen_simulation.md specifies bounded sparse GPU cache, 1/2/4-screen bands, same-world-clock catch-up, and conservative cross-band flux. Runtime pressure budget foundation completed in dun-6eyf. Core offscreen residency/flux scheduling remains open.
