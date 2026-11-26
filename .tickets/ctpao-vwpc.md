---
id: ctpao-vwpc
status: open
deps: []
links: [ctcri-jd9z]
created: 2026-11-26T23:11:09Z
type: bug
priority: 2
assignee: kmd
parent: ct-exwr
tags: [player, sdl, presentation]
---
# Keep gameplay usable when a local join cannot lay out split cameras

SDL Start commits a new seat before split layout validation. A drawable too small for the integer-scaled crops can reject the split layout and put its error into the sticky player failure buffer.

## Acceptance Criteria

Exercise a join when crop layout is unavailable, preserve existing gameplay and occupied seats, and allow the desired split view to recover when drawable dimensions are usable. Keep normal Start split behavior and controller reassignment.

