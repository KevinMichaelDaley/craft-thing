---
id: dun-abfx
status: in_progress
deps: []
links: []
created: 2026-09-26T06:24:48Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [world, gpu, streaming]
---
# Anchor pending GPU transfers to world chunks during camera streaming

The diagnostic transfer resolver uses viewport-local coordinates and one pending command. Bind queued transfers to signed world chunk coordinates and generations, pin source/destination slots, and retry safely after camera movement and slot reuse. Batch later fluid/sand proposals without races.

## Acceptance Criteria

A transfer queued toward an absent world chunk remains associated with that chunk while camera origin changes; slot reuse cannot redirect it; a streamed-window end-to-end test loads the destination and verifies exactly-once application.


## Notes

**2026-09-26T09:13:38Z**

The camera now has a one-chunk simulated halo; integrate deferred GPU water transfers at the outer resident edge so motion exceeding the halo distance is not stopped by missing pages.
