---
id: ctpao-vwpc
status: closed
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

## Notes

**2026-11-26T23:25:31Z**

The two host RED cases compiled and reproduced both failure paths. Start admitted two seats but wrote the refused layout into the sticky error and stopped player steps. A rejected refresh of an already active layout returned failure with both stale bands still published. The native small-drawable fixture compiled.

**2026-11-26T23:28:25Z**

GREEN and refactor passed all 16 production session cases, also under ASan/UBSan. The rejected Start layout leaves both seats usable and retries without a second Start. A later rejection removes stale bands, keeps the caller diagnostic untouched and maps controller aim through the single view before restoring its own band. Explicitly clearing the request stops retry. Five focus and five split geometry checks passed. Both native fixtures compiled and the exact displayed replays remain queued.

**2026-11-26T23:51:52Z**

Exact 0e7d3a04 reproduced the drawable rejection in native RED, then passed live recovery on the same simulated small logical extent. The separate two-window and controller rejoin/normal Start checks passed 2/2 each. Whole-suite verification remains queued.
